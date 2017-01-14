#include <ai.h>
#include <algorithm>
#include <string.h>
#include "alUtil.h"
#include <assert.h>
#include <stdio.h>
#include "fuse.h"
extern const char* SHADER_NAMES[];

typedef AtRGB (*FuseOperater)(const AtRGB &a, const AtRGB &b, float w);

AI_SHADER_NODE_EXPORT_METHODS(FuseShadingMtd);

const char* BLEND_MODES[] = {"Mix", "Add", "Multiply", "Max", "Min"};

node_parameters
{
   AddFuseParam;
   AiParameterEnum("blend_mode", 1, BLEND_MODES);
}


enum FuseShadingParams
{
   p_blendMode = NFuseParams
};


struct FuseShadingData: FuseData
{
   FuseOperater op;
};


node_initialize
{
   FuseShadingData* data = (FuseShadingData*)AiMalloc(sizeof(FuseShadingData));
   data->sampler = NULL;
   AiNodeSetLocalData(node, data);
}


AtRGB add(const AtRGB &a, const AtRGB &b, float w){
   return a + (b-a*b)*w;
}


AtRGB mul(const AtRGB &a, const AtRGB &b, float w){
   return lerp(a, a*b, w);
}


AtRGB mix(const AtRGB &a, const AtRGB &b, float w){
   return lerp(a, b, w*0.5); // the weight in the half is 0.5
}


inline float fastBrightness(const AtRGB &c){
   return c.r+c.g+c.b;
}

AtRGB _max(const AtRGB &a, const AtRGB &b, float w){
   return lerp(a, fastBrightness(a)>fastBrightness(b)?a:b, w);
}

AtRGB _min(const AtRGB &a, const AtRGB &b, float w){
   return lerp(a, fastBrightness(a)<fastBrightness(b)?a:b, w);
}

node_update
{
   UpdateFuseData;

   FuseShadingData* data = (FuseShadingData*)AiNodeGetLocalData(node);
   FuseOperater modes[] = {mix, add, mul, _max, _min};
   data->op = modes[params[p_blendMode].INT];
}


node_finish
{
   if (AiNodeGetLocalData(node)){
      FuseShadingData* data = (FuseShadingData*)AiNodeGetLocalData(node);
      AiSamplerDestroy(data->sampler);

      AiFree((void*)data);
      AiNodeSetLocalData(node, NULL);
   }
}

//TODO: a more elegant way rather than repeating
shader_evaluate
{
   FuseShadingData* data = (FuseShadingData*)AiNodeGetLocalData(node);

   if (data->shader == NULL)
   {
      if (data->mode != kVisRadius){
         sg->out.RGBA = data->shader_value;
         return;
      }
      else if (!(sg->Rt & AI_RAY_CAMERA)) {
         sg->out.RGBA = AI_RGBA_BLACK;
         return;
      }
    }

   if (data->enable == false || data->radius == 0.0f || !(sg->Rt & AI_RAY_CAMERA))
   {
      AiShaderEvaluate(data->shader, sg);
      if (data->shader_is_rgba == false)
         sg->out.RGBA.a = 1.f;
      return;
   }

   int blend_layer = getBlendLayer(sg);
   bool respect_layer = data->blend_layer;
   AtPoint Pref;
   if (!AiUDataGetPnt(REST_ATTR_NAME, &Pref)){
      data->min_blend_dist = 0.f;
      AiMsgWarning("[%s] %s has no Rest data (%s).", SHADER_NAMES[kShaderIdFuseNormal], AiNodeGetName(sg->Op), REST_ATTR_NAME);
   }

   AtVector Nn = AiV3Normalize(sg->N),
            tangent = AiV3Normalize(sg->dPdu),
            bitangent = AiV3Normalize(sg->dPdv);

   // evaluate to get AOV and initial color
   // TODO: delay evaluation to combine normal and shading?
   AtRGB __source_color = AI_RGB_BLACK;      // vialet 
   if (data->mode != kVisRadius) {
      AiShaderEvaluate(data->shader, sg);
      if (!data->shader_is_rgba)
            sg->out.RGBA.a = 1.f;
      __source_color = sg->out.RGB;
   } 
   const AtRGB source_color = __source_color;

   float offset_dist = data->adv_offsetScale*data->radius;
   AtRay near_test_ray;
   AtScrSample near_hitpoint;
   AiMakeRay(&near_test_ray, AI_RAY_DIFFUSE, &sg->P, &sg->N, offset_dist, sg);
   float in_radius = 0.f;

   // if too close, just blend.
   if (AiTrace(&near_test_ray, &near_hitpoint)){
      in_radius = 1.f;
   } else {

      AtRay ray;
      float samples[2];
      AtSamplerIterator *samit = AiSamplerIterator(data->sampler, sg);

      AtShaderGlobals hitpoint;
      AtScrSample hitcolor;
      AtRGB rgb_total = AI_RGB_BLACK;
      AtVector N_total = {0.f, 0.f, 0.f};
      AtPoint orig = sg->P + Nn*offset_dist;
      int n = 0;
      float trace_dist = data->radius*TRACE_DISTANCE_SCALE;
      float weight_total = 0.f;

      while(AiSamplerGetSample(samit, samples)) {
         /* ==== make the sample ray ==== */
         // try disk
         float u, v;
         concentricSampleDisk(samples[0], samples[1], u, v);

         AtVector o, dir = u*tangent + v*bitangent;
         dir = AiV3Normalize(dir);

         AiMakeRay(&ray, AI_RAY_DIFFUSE, &orig, &dir, trace_dist, sg);

         /* ==== gather ==== */
         if (AiTraceProbe(&ray, &hitpoint)) {
            bool layer_result = shouldBlendLayer(blend_layer, respect_layer, &hitpoint);

            if(!layer_result)
               continue;

            AtVector Nn_hitpoint = AiV3Normalize(hitpoint.N), L = hitpoint.P - orig;
            float cos_alpha = AiV3Dot(Nn, -Nn_hitpoint);
            float d;

            if (AiV3Dot(hitpoint.Ng, L)>0){
               AiMsgWarning("[%s] OHYEAH? Invalid area for fusing is detected.", SHADER_NAMES[kShaderIdFuseNormal]);

               if (data->mode == kMarkInvalid){
                  sg->out.RGBA.r= sg->out.RGBA.a = 1;
                  return;
               }else{
                  continue;
               }
            }

            float d1 = AiV3Length(L);
            float sin_alpha = sqrt(1.0001f+cos_alpha*cos_alpha); // use 1.0001 instead of 1.0 to tolerant float error.
            float d_pt2edge = offset_dist / sin_alpha;
            float d2 = sqrt(d_pt2edge*d_pt2edge + offset_dist*offset_dist);
            d2 *= cos_alpha>0?1:-1;
            d = d1+d2;

            // beyound radius
            if (d>data->radius)
               continue;

            float weight = CLAMP((1.f - d/data->radius)*(1+WEIGHT_ADD), 0.f, 1.0f);
            weight = bias(weight, data->weight_curve);
            weight_total += weight;


            if (data->mode != kVisRadius){
               AtByte Rt = AI_RAY_DIFFUSE;
               ray.type = Rt;
               sg->Rt = Rt;
               AiMakeRay(&ray, Rt, &orig, &dir, trace_dist, sg);   // specular is visible
               ray.type = AI_RAY_DIFFUSE;                          // not affect aovs
               AiTrace(&ray, &hitcolor);

               rgb_total += data->op(source_color, hitcolor.color, weight);
            }
            n++;
         }
      } // gather samples
      
      if (n) {
         sg->out.RGB = rgb_total/n;
         in_radius = weight_total/n;
      }
   }

   if (data->mode == kVisRadius){
      sg->out.RGB = rgb(in_radius);
      sg->out.RGBA.a = 1.0;
   } 
}
