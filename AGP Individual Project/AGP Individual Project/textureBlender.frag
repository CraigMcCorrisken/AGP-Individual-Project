#version 330

in vec2 tex_coord0;
in vec2 tex_coord1;

layout (location = 0) out vec4 color;

uniform sampler2D tex1;
uniform sampler2D tex2;

void main(void)
{
	color = (0.2*texture(tex1, tex_coord0) + 0.8*texture(tex2, tex_coord1));
	color.a = 1.0;
}