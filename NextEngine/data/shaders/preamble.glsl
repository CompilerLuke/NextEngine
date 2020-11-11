#version 440 core

#define CHANNEL1_UNIFORM(name) #ifndef name##_scalar \
uniform sampler2D name; \
#else \
uniform float name; \
#endif 

#define CHANNEL2_UNIFORM(name) #ifndef name##_scalar \
uniform sampler2D name; \
#else \
uniform vec2 name; \
#endif 

#define CHANNEL3_UNIFORM(name) #ifndef name##_scalar \
uniform sampler2D name; \
#else \
uniform vec3 name; \
#end if 
