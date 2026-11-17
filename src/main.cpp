// THE OSSUARY: Ashvald — entry point. Window creation, OpenGL 3.3 Core context and launch options.
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "app.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
// Prefer the discrete GPU on Optimus laptops; Windows per-app preferences take priority.
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
}
#endif

static void usage(){
    std::cerr<<"Usage: finalprj [--smoke-test] [--benchmark] [--capture out.ppm] [--zone id] [--class warrior|rogue|mage]\n"
               "               [--battle] [--menu] [--no-technique] [--time seconds] [--view close|wide] [--window name]\n";
}
int main(int argc,char** argv){
    LaunchOptions o;
    for(int i=1;i<argc;++i){
        std::string a=argv[i];
        auto next=[&]()->std::string{if(i+1>=argc)throw std::invalid_argument("missing value for "+a);return argv[++i];};
        try{
            if(a=="--smoke-test")o.smoke=true;
            else if(a=="--benchmark")o.benchmark=true;
            else if(a=="--capture")o.capture=std::filesystem::absolute(next());
            else if(a=="--zone")o.zone=next();
            else if(a=="--class")o.cls=next();
            else if(a=="--battle")o.battle=true;
            else if(a=="--menu")o.menu=true;
            else if(a=="--no-technique")o.technique=false;
            else if(a=="--time")o.time=std::stof(next());
            else if(a=="--view")o.view=next();
            else if(a=="--window")o.window=next();
            else{usage();return 1;}
        }catch(const std::exception& e){std::cerr<<e.what()<<'\n';usage();return 1;}
    }
    if(!o.capture.empty()&&o.zone.empty()&&!o.menu)o.menu=true;
    if(!o.zone.empty()&&!rpg::zoneDef(o.zone)){std::cerr<<"Unknown zone: "<<o.zone<<'\n';return 1;}
    glfwSetErrorCallback([](int code,const char* message){std::cerr<<"GLFW error "<<code<<": "<<message<<'\n';});
    if(!glfwInit()){std::cerr<<"GLFW initialisation failed\n";return 1;}
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GL_TRUE);
#endif
    bool hidden=o.smoke||o.benchmark||!o.capture.empty();
    if(hidden)glfwWindowHint(GLFW_VISIBLE,GL_FALSE);
    int w=o.capture.empty()?(o.benchmark?1280:1440):1920,h=o.capture.empty()?(o.benchmark?720:900):1080;
    GLFWwindow* window=glfwCreateWindow(w,h,"THE OSSUARY: Эшвальд",nullptr,nullptr);
    if(!window){std::cerr<<"Could not create an OpenGL 3.3 Core window. Update the graphics driver.\n";glfwTerminate();return 1;}
    glfwMakeContextCurrent(window);
    if(!gladLoadGL(glfwGetProcAddress)||!GLAD_GL_VERSION_3_3){std::cerr<<"OpenGL 3.3 Core is required\n";glfwDestroyWindow(window);glfwTerminate();return 1;}
    std::cout<<"OpenGL: "<<glGetString(GL_VERSION)<<"\nGPU: "<<glGetString(GL_RENDERER)<<'\n';
    int result=1;
    try{
        App app(window,std::filesystem::absolute(argv[0]).parent_path(),o);
        result=app.run();
    }catch(const std::exception& e){
        std::cerr<<"Fatal error: "<<e.what()<<'\n';
    }
    glfwDestroyWindow(window);glfwTerminate();
    return result;
}
