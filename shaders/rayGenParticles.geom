#version 450
layout (points) in;
layout (triangle_strip, max_vertices = 14) out;

precision highp float;

layout(location = 0) in VS_OUT {
         vec2 old_uv;
        float size;
    flat uint mat;
} gs_in[];

layout(location = 0)      out vec2 old_uv;
layout(location = 1) flat out vec3 norm;
layout(location = 2) flat out uint mat;

layout(binding = 0, set = 0) uniform UniformBufferObject {
    mat4 trans_w2s;
    mat4 trans_w2s_old;
} ubo;

const vec3 cube_strip[14] = {
    vec3(-1, +1, +1), // Front - top    - left
    vec3(+1, +1, +1), // Front - top    - right
    vec3(-1, -1, +1), // Front - bottom - left
    vec3(+1, -1, +1), // Front - bottom - right
    vec3(+1, -1, -1), // Back  - bottom - right
    vec3(+1, +1, +1), // Front - top    - right
    vec3(+1, +1, -1), // Back  - top    - right
    vec3(-1, +1, +1), // Front - top    - left
    vec3(-1, +1, -1), // Back  - top    - left
    vec3(-1, -1, +1), // Front - bottom - left
    vec3(-1, -1, -1), // Back  - bottom - left
    vec3(+1, -1, -1), // Back  - bottom - right
    vec3(-1, +1, -1), // Back  - top    - left
    vec3(+1, +1, -1), // Back  - top    - right
};
const vec3 norms[14] = {
    vec3(0,0,+1), // Front - top    - left
    vec3(0,0,+1), // Front - top    - right
    vec3(0,-1,0), // Front - bottom - left
    vec3(+1,0,0), // Front - bottom - right
    vec3(+1,0,0), // Back  - bottom - right
    vec3(0,+1,0), // Front - top    - right
    vec3(0,+1,0), // Back  - top    - right
    vec3(-1,0,0), // Front - top    - left
    vec3(-1,0,0), // Back  - top    - left
    vec3(0,-1,0), // Front - bottom - left
    vec3(0,0,-1), // Back  - bottom - left
    vec3(0,0,-1), // Back  - bottom - right
    //unused:
    vec3(1, 1, 1), // Back  - top    - left
    vec3(1, 1, 1), // Back  - top    - right
};

//fuck we need normals so 24 not 14

void main() {
    //so we are given particle center and its old_uv
    // clip = mat * pos
    // so we need to add\sub mat*0.5

    // mat3 m2w_normals = transpose(inverse(mat3(ubo.trans_m2w))); 

    // #pragma unroll
    for(int i=0; i<14; i++){
        vec3 shift_in_world  = cube_strip[i]*gs_in[0].size;
        vec3 shift_on_screen = (ubo.trans_w2s * vec4(shift_in_world,1)).xyz; //todo move out
        shift_on_screen.z = -shift_on_screen.z;
        // shift_on_screen = -vec3(.1);

        norm = norms[i];

        gl_Position = gl_in[0].gl_Position + vec4(shift_on_screen,0);
        // gl_Position.z = -.999;
        // gl_Position.z = +.999;
        
        old_uv = gs_in[0].old_uv;
        // old_uv = vec2(0);
        mat = gs_in[0].mat;

        EmitVertex();
    }

    // gl_Position = gl_in[0].gl_Position + vec4(+.5, +.5, 0.1, 0);
    // EmitVertex();
    // gl_Position = gl_in[0].gl_Position + vec4(-.5, +.5, 0.1, 0);
    // EmitVertex();
    // gl_Position = gl_in[0].gl_Position + vec4(-.5, -.5, 0.1, 0);
    // EmitVertex();
    // gl_Position = gl_in[0].gl_Position + vec4(+.5, -.5, 0.1, 0);
    // EmitVertex();


    EndPrimitive();
}

