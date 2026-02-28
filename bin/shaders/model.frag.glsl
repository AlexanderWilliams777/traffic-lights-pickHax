#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

uniform vec3 uColor;

out vec4 FragColor;

void main() {
    vec3 N = normalize(Normal);
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(N, lightDir), 0.0);
    vec3 diffuse = diff * uColor;
    vec3 ambient = 0.2 * uColor;
    FragColor = vec4(ambient + diffuse, 1.0);
}
