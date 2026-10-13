 #version 330 core
 layout (location = 0) in vec3 aPos;
 layout (location = 1) in vec2 tex;
layout (location = 2) in vec3 norm;
layout (location = 3) in vec2 uRange; // atlas [uMin, uMax] this face's sub-texture occupies


 uniform mat4 model;
 uniform mat4 projection;
 uniform mat4 view;

 out vec4 vCol;
 out vec2 texCoord0;
out vec3 Normal;
out vec3 fragPos; //fragment position (camera and shit)
out vec2 uRangeOut;


 void main() {
 gl_Position = projection * view * model * vec4(aPos, 1.0);
 vCol = vec4(clamp(aPos, 0.0f, 1.0f), 1.0f);
 texCoord0 = tex;
 uRangeOut = uRange;

Normal = mat3(transpose(inverse(model))) * norm;

fragPos = (model * vec4(aPos, 1.0)).xyz;

 }