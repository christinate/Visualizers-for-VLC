#pragma once
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif
#include <stdexcept>
#include <string>
namespace native {
class PixelPresenter {
    GLuint program=0,texture=0,vao=0;
    static GLuint shader(GLenum type,const char* text){
        GLuint value=glCreateShader(type);glShaderSource(value,1,&text,nullptr);glCompileShader(value);GLint ok=0;glGetShaderiv(value,GL_COMPILE_STATUS,&ok);
        if(!ok){char log[2048]{};glGetShaderInfoLog(value,sizeof(log),nullptr,log);glDeleteShader(value);throw std::runtime_error(std::string("Shader compilation: ")+log);}return value;
    }
public:
    PixelPresenter(){
        const char* vertex="#version 330\nout vec2 uv;void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=vec2(p.x,1.-p.y);gl_Position=vec4(p*2.-1.,0.,1.);}";
        const char* pixel="#version 330\nin vec2 uv;out vec4 color;uniform sampler2D image;void main(){color=texture(image,uv);}";
        auto v=shader(GL_VERTEX_SHADER,vertex),f=shader(GL_FRAGMENT_SHADER,pixel);program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);glLinkProgram(program);glDeleteShader(v);glDeleteShader(f);
        GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);if(!ok)throw std::runtime_error("Pixel presenter program link failed");
        glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glGenVertexArrays(1,&vao);
    }
    ~PixelPresenter(){if(vao)glDeleteVertexArrays(1,&vao);if(texture)glDeleteTextures(1,&texture);if(program)glDeleteProgram(program);}
    void draw(const void* pixels,int width,int height){glBindFramebuffer(GL_FRAMEBUFFER,0);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glUseProgram(program);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_BGRA,GL_UNSIGNED_BYTE,pixels);glUniform1i(glGetUniformLocation(program,"image"),0);glBindVertexArray(vao);glDrawArrays(GL_TRIANGLES,0,3);}
};
}
