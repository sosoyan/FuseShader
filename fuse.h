#pragma once

#define AddFuseParam addFuseParam(params, mds)
#define UpdateFuseData updateFuseData(node, params)
#define QUICK_CHECK(_v)    {sg->out.RGB = rgb(_v); sg->out.RGBA.a = 1.0; return;}


const int DEFUALT_LAYER = INT_MAX;                 // when no layer is assigned
const int SUPER_LAYER = -4095;                     // the layer that blend all layer.

static const float TRACE_DISTANCE_SCALE = 4.f; 
//static const float BIAS_DISTANCE_SCALE = 0.15f;  // move along N
static const float WEIGHT_ADD = 0.05f;             // weight threshold to become 1
static const char* cModes[] = {"Render", "Mark Invalid", "Visualize Radius", 0};
static const char* REST_ATTR_NAME = "Pref";


enum Mode{
   kRender=0,
   kMarkInvalid,
   kVisRadius
};

enum ShaderId
{
   kShaderIdFuseNormal=0,
   kShaderIdFuseShading
};


enum FuseParams {
   p_enable,
   p_radius,
   p_minBlendDist,
   p_blendLayer,
   p_sample,
   p_weightCurve,
   p_shader,
   p_renderMode,
   p_advOffsetScale,
   NFuseParams
};


struct FuseData{
   bool  enable, blend_layer;
   float radius, min_blend_dist, weight_curve, adv_offsetScale;
   int   nsamples, mode;
   AtNode* shader;
   AtSampler* sampler;
   AtRGBA shader_value;
   bool shader_is_rgba;
};


inline int getBlendLayer(const AtShaderGlobals* sg){
   // return INT_MAX if no layer specified.//
   int layer;
   if (!AiUserGetIntFunc("blend_layer", sg, &layer))
      return DEFUALT_LAYER;
   return layer;
}


// align with rgb(AtVector) and rgb(float)
inline AtRGB rgb(const AtRGB& c)
{
    return c;
}


inline bool shouldBlendLayer(int layer, bool respect_layer, const AtShaderGlobals* hitpoint){
   int blend_layer = getBlendLayer(hitpoint);
   if (!respect_layer or layer == SUPER_LAYER or blend_layer== SUPER_LAYER)
      return true;
   else
      return (layer == blend_layer);
}

void addFuseParam(AtList* params, AtMetaDataStore* mds);

void updateFuseData(AtNode* node, AtParamValue* params);