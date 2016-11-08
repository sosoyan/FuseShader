#include <ai.h>
#include <algorithm>
#include <string.h>
#include <alUtil.h>
#include <assert.h>
#include <stdio.h>
#include "fuse.h"
extern const char* SHADER_NAMES[];


AI_SHADER_NODE_EXPORT_METHODS(FuseNormalMtd);

/*
export ARNOLD_PATH='/mnt/usr/Arnold-4.2.2.0-linux'


g++ -o /mnt/public/home/john/git/shader/blended/tmp/shader.os -c -fPIC -D_LINUX -D_DEBUG -I$ARNOLD_PATH/include -I/mnt/public/home/john/arnoldshader_src/alShaders-src-0.3.3/common  /mnt/public/home/john/git/shader/blended/enamel.cpp ;
g++ -o /mnt/public/home/john/git/shader/blended/tmp/shader.so -shared /mnt/public/home/john/git/shader/blended/tmp/shader.os -L$ARNOLD_PATH/bin -lai;

export ARNOLD_SHADERLIB_PATH='/mnt/public/home/john/git/shader/blended/tmp'
*/


node_parameters
{
   AddFuseParam;
}


node_initialize
{
   FuseData* data = (FuseData*)AiMalloc(sizeof(FuseData));
   data->sampler = NULL;
   AiNodeSetLocalData(node, data);
}


node_update
{
   UpdateFuseData;
}


node_finish
{
   if (AiNodeGetLocalData(node)){
      FuseData* data = (FuseData*)AiNodeGetLocalData(node);
      AiSamplerDestroy(data->sampler);

      AiFree((void*)data);
      AiNodeSetLocalData(node, NULL);
   }
}


shader_evaluate
{
   FuseData* data = (FuseData*)AiNodeGetLocalData(node);

   if (data->shader == NULL)
      if (data->mode != kVisRadius){
         sg->out.RGBA = data->shader_value;
         return;
      }
      else if (!(sg->Rt & AI_RAY_CAMERA)) {
         sg->out.RGBA = AI_RGBA_BLACK;
         return;
      }

   if (data->enable == false || data->radius == 0.0f || !(sg->Rt & AI_RAY_CAMERA))
   {
      AiShaderEvaluate(data->shader, sg);
      if (data->shader_is_rgba == false)
         sg->out.RGBA.a = 1.f;
      return;
   }



   AtVector oldN = sg->N;
   AtVector oldNf = sg->Nf;
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

   float offset_dist = data->adv_offsetScale*data->radius;
   AtRay near_test_ray;
   AtShaderGlobals near_hitpoint;
   AiMakeRay(&near_test_ray, AI_RAY_DIFFUSE, &sg->P, &sg->N, offset_dist, sg);
   float in_radius = 0.f;

   // if too close, just blend.
   if (AiTraceProbe(&near_test_ray, &near_hitpoint)){
      sg->Nf = sg->N = AiV3Normalize(AiV3Normalize(near_hitpoint.N)+Nn);
      AiFaceViewer(sg, sg->Nf);
      in_radius = 1.0;
   } else {
      AtRay ray;
      float samples[2];
      AtSamplerIterator *samit = AiSamplerIterator(data->sampler, sg);

      AtShaderGlobals hitpoint;
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
         AiMakeRay(&ray, AI_RAY_DIFFUSE , &orig, &dir, trace_dist, sg);

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
               AiFaceForward(&hitpoint.N, -hitpoint.Ng); // use Ng's sign, but N's direction;
               N_total += AiV3Normalize(hitpoint.N)*weight;
            }
            n += 1;
         }
      } // gather samples

      if (n) {
         sg->Nf = sg->N = AiV3Normalize(N_total/n+Nn);
         AiFaceViewer(sg, sg->Nf);
         in_radius = weight_total/n;
      }
   }

   if (data->mode == kVisRadius){
      sg->out.RGB = rgb(in_radius);
      sg->out.RGBA.a = 1.0;
   } else {
      AiShaderEvaluate(data->shader, sg);
      if (!data->shader_is_rgba)
         sg->out.RGBA.a = 1.f;
   }

   sg->N = oldN;
   sg->Nf = oldNf;
}
