#pragma once

#include <array>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <gfx/research/shader.hpp>
#include <glad/gl.h>

class CloudNoise {
public:
    ~CloudNoise() { if (textures_[0]) glDeleteTextures(2, textures_.data()); if(bounds_) glDeleteTextures(1,&bounds_); }
    bool create(const std::filesystem::path& shaderPath) {
        const gfx::research::ShaderFile file{gfx::research::ShaderStage::Compute, shaderPath};
        gfx::research::Shader generator;
        if (!generator.load(std::span{&file, 1})) return false;
        glCreateTextures(GL_TEXTURE_3D, 2, textures_.data());
        for (int i=0; i<2; ++i) {
            glTextureStorage3D(textures_[i], 1, GL_RG8, 128, 128, 128);
            glTextureParameteri(textures_[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTextureParameteri(textures_[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(textures_[i], GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTextureParameteri(textures_[i], GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTextureParameteri(textures_[i], GL_TEXTURE_WRAP_R, GL_REPEAT);
            glBindImageTexture(i, textures_[i], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RG8);
        }
        generator.bind();
        glDispatchCompute(16, 16, 16);
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        // Finish one-time resource generation before any benchmark timestamps.
        glFinish();
        glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG8);
        glBindImageTexture(1, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG8);
        // Build conservative interpolation-cell maxima. Texture is periodic, so the
        // last cell includes vertices from the first texel. No average mipmaps.
        std::vector<std::uint8_t> pixels(128*128*128*2), level(128*128*128);
        glGetTextureImage(textures_[0],0,GL_RG,GL_UNSIGNED_BYTE,static_cast<GLsizei>(pixels.size()),pixels.data());
        auto index=[](int x,int y,int z,int n){return (z*n+y)*n+x;};
        for(int z=0;z<128;++z)for(int y=0;y<128;++y)for(int x=0;x<128;++x){
            std::uint8_t m=0;
            for(int dz=0;dz<2;++dz)for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx)
                m=std::max(m,pixels[2*index((x+dx)%128,(y+dy)%128,(z+dz)%128,128)]);
            level[index(x,y,z,128)]=m;
        }
        glCreateTextures(GL_TEXTURE_3D,1,&bounds_);
        glTextureStorage3D(bounds_,8,GL_R8,128,128,128);
        glTextureParameteri(bounds_,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_NEAREST);
        glTextureParameteri(bounds_,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        GLint alignment=4; glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        for(int mip=0,n=128;mip<8;++mip,n/=2){
            glTextureSubImage3D(bounds_,mip,0,0,0,n,n,n,GL_RED,GL_UNSIGNED_BYTE,level.data());
            if(n==1)break;
            int half=n/2;std::vector<std::uint8_t> next(half*half*half);
            for(int z=0;z<half;++z)for(int y=0;y<half;++y)for(int x=0;x<half;++x){
                std::uint8_t m=0;
                for(int dz=0;dz<2;++dz)for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx)
                    m=std::max(m,level[index(2*x+dx,2*y+dy,2*z+dz,n)]);
                next[index(x,y,z,half)]=m;
            }
            level.swap(next);
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);glFinish();
        return true;
    }
    void bind() const { glBindTextureUnit(4, textures_[0]); glBindTextureUnit(5, textures_[1]); glBindTextureUnit(6,bounds_); }
private:
    std::array<GLuint, 2> textures_{};
    GLuint bounds_=0;
};
