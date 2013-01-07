/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "OpenGLRenderer"

#include "Program.h"

namespace android {
namespace uirenderer {

///////////////////////////////////////////////////////////////////////////////
// Base program
///////////////////////////////////////////////////////////////////////////////

Program::Program(const char* vertex, const char* fragment) {
    mInitialized = false;
    mHasColorUniform = true;

    // No need to cache compiled shaders, rely instead on Android's
    // persistent shaders cache
    GLuint vertexShader = buildShader(vertex, GL_VERTEX_SHADER);
    if (vertexShader) {

        GLuint fragmentShader = buildShader(fragment, GL_FRAGMENT_SHADER);
        if (fragmentShader) {

            mProgramId = glCreateProgram();
            glAttachShader(mProgramId, vertexShader);
            glAttachShader(mProgramId, fragmentShader);
            glLinkProgram(mProgramId);

            GLint status;
            glGetProgramiv(mProgramId, GL_LINK_STATUS, &status);
            if (status != GL_TRUE) {
                LOGE("Error while linking shaders:");
                GLint infoLen = 0;
                glGetProgramiv(mProgramId, GL_INFO_LOG_LENGTH, &infoLen);
                if (infoLen > 1) {
                    GLchar log[infoLen];
                    glGetProgramInfoLog(mProgramId, infoLen, 0, &log[0]);
                    LOGE("%s", log);
                }
            } else {
                mInitialized = true;
            }
            
            glDetachShader(mProgramId, vertexShader);
            glDetachShader(mProgramId, fragmentShader);

            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);

            if (!mInitialized) {
                glDeleteProgram(mProgramId);
            }
        } else {
            glDeleteShader(vertexShader);
        }
    }

    mUse = false;

    if (mInitialized) {
        position = addAttrib("position");
        transform = addUniform("transform");
    }
}

Program::~Program() {
    if (mInitialized) {
        glDeleteProgram(mProgramId);
    }
}

int Program::addAttrib(const char* name) {
    int slot = glGetAttribLocation(mProgramId, name);
    mAttributes.add(name, slot);
    return slot;
}

int Program::getAttrib(const char* name) {
    ssize_t index = mAttributes.indexOfKey(name);
    if (index >= 0) {
        return mAttributes.valueAt(index);
    }
    return addAttrib(name);
}

int Program::addUniform(const char* name) {
    int slot = glGetUniformLocation(mProgramId, name);
    mUniforms.add(name, slot);
    return slot;
}

int Program::getUniform(const char* name) {
    ssize_t index = mUniforms.indexOfKey(name);
    if (index >= 0) {
        return mUniforms.valueAt(index);
    }
    return addUniform(name);
}

GLuint Program::buildShader(const char* source, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, 0);
    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        // Some drivers return wrong values for GL_INFO_LOG_LENGTH
        // use a fixed size instead
        GLchar log[512];
        glGetShaderInfoLog(shader, sizeof(log), 0, &log[0]);
        LOGE("Error while compiling shader: %s", log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

void Program::set(const mat4& projectionMatrix, const mat4& modelViewMatrix,
        const mat4& transformMatrix, bool offset) {
    mat4 t(projectionMatrix);
    if (offset) {
        // offset screenspace xy by an amount that compensates for typical precision
        // issues in GPU hardware that tends to paint hor/vert lines in pixels shifted
        // up and to the left.
        // This offset value is based on an assumption that some hardware may use as
        // little as 12.4 precision, so we offset by slightly more than 1/16.
        t.translate(.375, .375, 0);
    }
    t.multiply(transformMatrix);
    t.multiply(modelViewMatrix);

    glUniformMatrix4fv(transform, 1, GL_FALSE, &t.data[0]);
}

void Program::setColor(const float r, const float g, const float b, const float a) {
    if (!mHasColorUniform) {
        mColorUniform = getUniform("color");
        mHasColorUniform = false;
    }
    glUniform4f(mColorUniform, r, g, b, a);
}

void Program::use() {
    glUseProgram(mProgramId);
    mUse = true;

    glEnableVertexAttribArray(position);
}

void Program::remove() {
    mUse = false;
    // TODO: Is this necessary? It should not be since all of our shaders
    //       use slot 0 for the position attrib
    // glDisableVertexAttribArray(position);
}

}; // namespace uirenderer
}; // namespace android
