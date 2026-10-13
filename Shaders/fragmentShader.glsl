#version 330 core
out vec4 FragColor;

in vec4 vCol;
in vec2 texCoord0;
in vec3 Normal;
in vec3 fragPos;
in vec2 uRangeOut;

struct directionalLight {
    vec3 colour;
    float ambientIntensity;
    vec3 direction;
    float diffuseIntensity;

};

struct Material {
    float specularIntensity;
    float shininess;
};


uniform sampler2D theTexture;
uniform directionalLight directLight;
uniform Material material;

uniform vec3 cameraPosition; // eye position

void main () {
    vec4 ambientColour = vec4(directLight.colour, 1.0f) * directLight.ambientIntensity;

    float diffuseFactor = max(dot(normalize(Normal), normalize(directLight.direction)), 0.0f);
    vec4 diffuseColour = vec4(directLight.colour, 1.0f) * directLight.diffuseIntensity * diffuseFactor;

    vec4 specularColor = vec4(0,0,0,0);

    if (diffuseFactor > 0.0f) { // if theres light hittign the object
        //getting the vector betweene eye position to the point of specular light
        vec3 fragToEye = normalize(cameraPosition - fragPos);
        vec3 reflectedVertex = normalize(reflect(directLight.direction, normalize(Normal)));

        float specularFactor = dot(fragToEye, reflectedVertex);

        if (specularFactor > 0.0f) {
            specularFactor = pow(specularFactor, material.shininess);
            specularColor = vec4(directLight.colour * material.specularIntensity * specularFactor, 1.0f);
        }
    }

    // texCoord0 is tile-space (repeats every 1.0 unit per block) for greedy-merged quads;
    // fract() wraps it and mix() maps it back into this face's atlas sub-rectangle.
    vec2 wrapped = fract(texCoord0);
    vec2 atlasUV = vec2(mix(uRangeOut.x, uRangeOut.y, wrapped.x), wrapped.y);

    FragColor = texture(theTexture, atlasUV) * (ambientColour + diffuseColour + specularColor);

}