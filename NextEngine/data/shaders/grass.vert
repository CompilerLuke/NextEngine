#include shaders/vert_helper.glsl

float psin(float low, float high, float x) {
	return (0.5 + 0.5 * sin(x)) * (high - low) + low;
}

void main()
{
    TexCoords = aTexCoords; // vec2(-aTexCoords.x * 0.96 - 0.04 , aTexCoords.y);

	/*vec3 ObjectCenter = vec3(model[3][0], model[3][1], model[3][2]);
	ObjectCenter *= 5.0;

	vec3 WindDir = vec3(1.0,-0.1,1.0) * 0.2;
	float speed = 5.0;
	
	//WindDir += aNormal * 2.0;
	//WindDir *= 0.6 + psin(0.0, 0.4, time);

	//pos.x += 0.05 * aPos.y * (2 * sin(1 * (ObjectCenter.x + ObjectCenter.y + ObjectCenter.z + time)));
	//pos.y += 0.05 * aPos.y * (1 * sin(2 * (ObjectCenter.x + ObjectCenter.y + ObjectCenter.z + time)));

	float BendFactor = max(aPos.y, 0.0);
	vec3 disp =  WindDir * BendFactor  * (0.5 + 0.5 * sin(ObjectCenter.x + time * speed));
	//disp.x = WindDir.x * BendFactor  * (0.5 + 0.5 * sin(ObjectCenter.x + time * speed));
	//disp.y = WindDir.y * BendFactor  * (0.5 + 0.5 * sin(2.0 + ObjectCenter.y + time * speed));
	//disp.z = WindDir.z * BendFactor  * (0.5 + 0.5 * sin(3.0 * ObjectCenter.z + time * speed));
*/

	vec4 pos = model * vec4(aPos,1.0); // + vec4(disp, 0.0);
    gl_Position = projection * view * pos;
	NDC = gl_Position.xyz / gl_Position.w;

#ifndef IS_DEPTH_ONLY
	FragPos = vec3(pos);

	TBN = make_TBN(model, aNormal, aTangent, aBitangent);
#endif 
}