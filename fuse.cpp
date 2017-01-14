#include <ai.h>
#include <algorithm>
#include <string.h>
#include "alUtil.h"
#include <assert.h>
#include <stdio.h>
#include "fuse.h"

extern AtNodeMethods* FuseNormalMtd;
extern AtNodeMethods* FuseShadingMtd;

const char* SHADER_NAMES[] = {"FuseNormal", "FuseShading"};
AtNodeMethods* MTDS[] = {FuseNormalMtd, FuseShadingMtd};

node_loader{
   switch(i){
      case 0:
         node->methods = MTDS[i];
         node->output_type = AI_TYPE_RGBA;
         node->name = SHADER_NAMES[i];
         node->node_type = AI_NODE_SHADER;
         strcpy(node->version, AI_VERSION);
         return true;
      case 1:
         node->methods = MTDS[i];
         node->output_type = AI_TYPE_RGBA;
         node->name = SHADER_NAMES[i];
         node->node_type = AI_NODE_SHADER;
         strcpy(node->version, AI_VERSION);
         return true;
      default:
         return false;
   }
}


void addFuseParam(AtList* params, AtMetaDataStore* mds){
   AiParameterBool("enable", true);
   AiParameterFlt("radius", 0.01f);
   AiParameterFlt("min_blend_dist", 0.1f);
   AiParameterBool("blend_layer", true);
   AiParameterInt("nsamples", 4);
   AiParameterFlt("blend_bias", 0.f);
   AiParameterRGBA("shader", 0.f, 0.f, 0.f, 1.f);
   AiParameterEnum("render_mode", 2, cModes);
   AiParameterFlt("adv_offsetScale", 0.15f);
}


void updateFuseData(AtNode* node, AtParamValue* params){
   FuseData* data = (FuseData*)AiNodeGetLocalData(node);
   data->enable = params[p_enable].BOOL;
   data->radius = params[p_radius].FLT;
   data->min_blend_dist = params[p_minBlendDist].FLT;
   data->blend_layer = params[p_blendLayer].BOOL;
   data->nsamples = params[p_sample].INT;
   data->weight_curve = params[p_weightCurve].FLT*0.5f+0.5f;   // map (-1,1) -> (0,1)
   data->mode = params[p_renderMode].INT;
   data->adv_offsetScale = params[p_advOffsetScale].FLT;

   if (ABS(data->radius) < AI_EPSILON)
      data->radius = 0.f;

   AtNode* options = AiUniverseGetOptions();
   if (AiNodeGetInt(options, "GI_diffuse_depth")==0)
      AiMsgWarning("%s requires GI_diffuse_depth set to at least 1, now 0.", SHADER_NAMES[kShaderIdFuseShading]);

   if (ABS(data->min_blend_dist) < AI_EPSILON)
      data->min_blend_dist = 0.f;

   data->shader = AiNodeGetLink(node, "shader");
   if (data->shader == NULL) {
      data->shader_value = AiNodeGetRGBA(node, "shader");
   } else {
      const AtNodeEntry* nent = AiNodeGetNodeEntry(data->shader);
      data->shader_is_rgba = AiNodeEntryGetOutputType(nent)==AI_TYPE_RGBA;
   }

   if (data->sampler) {
      AiSamplerDestroy(data->sampler);
   }
   data->sampler = AiSampler(data->nsamples, 2);
}
