uniform mat4 ModelMatrix;
uniform int skinning_offset;
uniform int layer;

#if __VERSION__ >= 330
layout(location = 0) in vec3 Position;
layout(location = 3) in vec4 Data1;
layout(location = 5) in ivec4 Joint;
layout(location = 6) in vec4 Weight;
#else
in vec3 Position;
in vec4 Data1;
in ivec4 Joint;
in vec4 Weight;
#endif

#ifdef VSLayer
out vec2 uv;
#else
out vec2 tc;
out int layerId;
#endif

void main(void)
{
    vec4 idle_position = vec4(Position, 1.);
    vec4 skinned_position = vec4(0.);
    for (int i = 0; i < 4; i++)
    {
        vec4 single_bone_influenced_position = joint_matrices[clamp(Joint[i] + skinning_offset, 0, MAX_BONES)] * idle_position;
        single_bone_influenced_position /= single_bone_influenced_position.w;
        skinned_position += Weight[i] * single_bone_influenced_position;
    }

#ifdef VSLayer
    gl_Layer = layer;
    uv = Data1.xy;
    gl_Position = ShadowViewProjMatrixes[gl_Layer] * ModelMatrix * skinned_position;
#else
    layerId = layer;
    tc = Data1.xy;
    gl_Position = ShadowViewProjMatrixes[layerId] * ModelMatrix * skinned_position;
#endif
}
