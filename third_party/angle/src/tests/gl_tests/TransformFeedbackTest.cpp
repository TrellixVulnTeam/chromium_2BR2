//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"

using namespace angle;

namespace
{

class TransformFeedbackTest : public ANGLETest
{
  protected:
    TransformFeedbackTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        const std::string vertexShaderSource = SHADER_SOURCE
        (
            precision highp float;
            attribute vec4 position;

            void main()
            {
                gl_Position = position;
            }
        );

        const std::string fragmentShaderSource = SHADER_SOURCE
        (
            precision highp float;

            void main()
            {
                gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
            }
        );

        mProgram = CompileProgram(vertexShaderSource, fragmentShaderSource);
        if (mProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }

        glGenBuffers(1, &mTransformFeedbackBuffer);
        mTransformFeedbackBufferSize = 1 << 24; // ~16MB
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, mTransformFeedbackBuffer);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, mTransformFeedbackBufferSize, NULL, GL_STATIC_DRAW);

        ASSERT_GL_NO_ERROR();
    }

    void TearDown() override
    {
        glDeleteProgram(mProgram);
        glDeleteBuffers(1, &mTransformFeedbackBuffer);
        ANGLETest::TearDown();
    }

    GLuint mProgram;

    size_t mTransformFeedbackBufferSize;
    GLuint mTransformFeedbackBuffer;
};

TEST_P(TransformFeedbackTest, ZeroSizedViewport)
{
    // Set the program's transform feedback varyings (just gl_Position)
    const GLchar* transformFeedbackVaryings[] =
    {
        "gl_Position"
    };
    glTransformFeedbackVaryings(mProgram,
                                static_cast<GLsizei>(ArraySize(transformFeedbackVaryings)),
                                transformFeedbackVaryings, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(mProgram);

    // Re-link the program
    GLint linkStatus;
    glGetProgramiv(mProgram, GL_LINK_STATUS, &linkStatus);
    ASSERT_NE(linkStatus, 0);

    glUseProgram(mProgram);

    // Bind the buffer for transform feedback output and start transform feedback
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, mTransformFeedbackBuffer);
    glBeginTransformFeedback(GL_TRIANGLES);

    // Create a query to check how many primitives were written
    GLuint primitivesWrittenQuery = 0;
    glGenQueries(1, &primitivesWrittenQuery);
    glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, primitivesWrittenQuery);

    // Set a viewport that would result in no pixels being written to the framebuffer and draw
    // a quad
    glViewport(0, 0, 0, 0);

    drawQuad(mProgram, "position", 0.5f);

    // End the query and transform feedkback
    glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
    glEndTransformFeedback();

    // Check how many primitives were written and verify that some were written even if
    // no pixels were rendered
    GLuint primitivesWritten = 0;
    glGetQueryObjectuiv(primitivesWrittenQuery, GL_QUERY_RESULT_EXT, &primitivesWritten);
    EXPECT_GL_NO_ERROR();

    EXPECT_EQ(2u, primitivesWritten);
}

// Test that XFB can write back vertices to a buffer and that we can draw from this buffer afterward.
TEST_P(TransformFeedbackTest, RecordAndDraw)
{
    // TODO(jmadill): Figure out why this fails on Intel.
    if (isIntel() && GetParam().getRenderer() == EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
    {
        std::cout << "Test skipped on Intel." << std::endl;
        return;
    }

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Set the program's transform feedback varyings (just gl_Position)
    const GLchar* transformFeedbackVaryings[] =
    {
        "gl_Position"
    };
    glTransformFeedbackVaryings(mProgram,
                                static_cast<GLsizei>(ArraySize(transformFeedbackVaryings)),
                                transformFeedbackVaryings, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(mProgram);

    // Re-link the program
    GLint linkStatus;
    glGetProgramiv(mProgram, GL_LINK_STATUS, &linkStatus);
    ASSERT_NE(linkStatus, 0);

    glUseProgram(mProgram);

    GLint positionLocation = glGetAttribLocation(mProgram, "position");

    // First pass: draw 6 points to the XFB buffer
    glEnable(GL_RASTERIZER_DISCARD);

    const GLfloat vertices[] =
    {
        -1.0f,  1.0f, 0.5f,
        -1.0f, -1.0f, 0.5f,
         1.0f, -1.0f, 0.5f,

        -1.0f,  1.0f, 0.5f,
         1.0f, -1.0f, 0.5f,
         1.0f,  1.0f, 0.5f,
    };

    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(positionLocation);

    // Bind the buffer for transform feedback output and start transform feedback
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, mTransformFeedbackBuffer);
    glBeginTransformFeedback(GL_POINTS);

    // Create a query to check how many primitives were written
    GLuint primitivesWrittenQuery = 0;
    glGenQueries(1, &primitivesWrittenQuery);
    glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, primitivesWrittenQuery);

    glDrawArrays(GL_POINTS, 0, 6);

    glDisableVertexAttribArray(positionLocation);
    glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, NULL);
    // End the query and transform feedkback
    glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
    glEndTransformFeedback();

    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);

    glDisable(GL_RASTERIZER_DISCARD);

    // Check how many primitives were written and verify that some were written even if
    // no pixels were rendered
    GLuint primitivesWritten = 0;
    glGetQueryObjectuiv(primitivesWrittenQuery, GL_QUERY_RESULT_EXT, &primitivesWritten);
    EXPECT_GL_NO_ERROR();

    EXPECT_EQ(6u, primitivesWritten);

    // Nothing should have been drawn to the framebuffer
    EXPECT_PIXEL_EQ(getWindowWidth() / 2, getWindowHeight() / 2, 0, 0, 0, 0);

    // Second pass: draw from the feedback buffer

    glBindBuffer(GL_ARRAY_BUFFER, mTransformFeedbackBuffer);
    glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(positionLocation);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    EXPECT_PIXEL_EQ(getWindowWidth() / 2, getWindowHeight() / 2, 255,   0,   0, 255);
    EXPECT_GL_NO_ERROR();
}

// Test that buffer binding happens only on the current transform feedback object
TEST_P(TransformFeedbackTest, BufferBinding)
{
    // Reset any state
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);

    // Generate a new transform feedback and buffer
    GLuint transformFeedbackObject = 0;
    glGenTransformFeedbacks(1, &transformFeedbackObject);

    GLuint scratchBuffer = 0;
    glGenBuffers(1, &scratchBuffer);

    EXPECT_GL_NO_ERROR();

    // Bind TF 0 and a buffer
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, mTransformFeedbackBuffer);

    EXPECT_GL_NO_ERROR();

    // Check that the buffer ID matches the one that was just bound
    GLint currentBufferBinding = 0;
    glGetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &currentBufferBinding);
    EXPECT_EQ(static_cast<GLuint>(currentBufferBinding), mTransformFeedbackBuffer);

    EXPECT_GL_NO_ERROR();

    // Check that the buffer ID for the newly bound transform feedback is zero
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformFeedbackObject);

    glGetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &currentBufferBinding);
    EXPECT_EQ(0, currentBufferBinding);

    EXPECT_GL_NO_ERROR();

    // Bind a buffer to this TF
    glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, scratchBuffer, 0, 32);

    glGetIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 0, &currentBufferBinding);
    EXPECT_EQ(static_cast<GLuint>(currentBufferBinding), scratchBuffer);

    EXPECT_GL_NO_ERROR();

    // Rebind the original TF and check it's bindings
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

    glGetIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 0, &currentBufferBinding);
    EXPECT_EQ(0, currentBufferBinding);

    EXPECT_GL_NO_ERROR();

    // Clean up
    glDeleteTransformFeedbacks(1, &transformFeedbackObject);
    glDeleteBuffers(1, &scratchBuffer);
}

// Test that we can capture varyings only used in the vertex shader.
TEST_P(TransformFeedbackTest, VertexOnly)
{
    const std::string &vertexShaderSource =
        "#version 300 es\n"
        "in vec2 position;\n"
        "in float attrib;\n"
        "out float varyingAttrib;\n"
        "void main() {\n"
        "  gl_Position = vec4(position, 0, 1);\n"
        "  varyingAttrib = attrib;\n"
        "}";

    const std::string &fragmentShaderSource =
        "#version 300 es\n"
        "out mediump vec4 color;\n"
        "void main() {\n"
        "  color = vec4(0.0, 1.0, 0.0, 1.0);\n"
        "}";

    std::vector<std::string> tfVaryings;
    tfVaryings.push_back("varyingAttrib");

    GLuint program = CompileProgramWithTransformFeedback(vertexShaderSource, fragmentShaderSource,
                                                         tfVaryings, GL_INTERLEAVED_ATTRIBS);
    ASSERT_NE(0u, program);

    GLuint transformFeedback;
    glGenTransformFeedbacks(1, &transformFeedback);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformFeedback);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, mTransformFeedbackBuffer);

    std::vector<float> attribData;
    for (unsigned int cnt = 0; cnt < 100; ++cnt)
    {
        attribData.push_back(static_cast<float>(cnt));
    }

    GLint attribLocation = glGetAttribLocation(program, "attrib");
    ASSERT_NE(-1, attribLocation);

    glVertexAttribPointer(attribLocation, 1, GL_FLOAT, GL_FALSE, 4, &attribData[0]);
    glEnableVertexAttribArray(attribLocation);

    glBeginTransformFeedback(GL_TRIANGLES);
    drawQuad(program, "position", 0.5f);
    glEndTransformFeedback();
    ASSERT_GL_NO_ERROR();

    GLvoid *mappedBuffer =
        glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(float) * 6, GL_MAP_READ_BIT);
    ASSERT_NE(nullptr, mappedBuffer);

    float *mappedFloats = static_cast<float *>(mappedBuffer);
    for (unsigned int cnt = 0; cnt < 6; ++cnt)
    {
        EXPECT_EQ(attribData[cnt], mappedFloats[cnt]);
    }

    glDeleteTransformFeedbacks(1, &transformFeedback);
    glDeleteProgram(program);

    EXPECT_GL_NO_ERROR();
}

// Test that multiple paused transform feedbacks do not generate errors or crash
TEST_P(TransformFeedbackTest, MultiplePaused)
{
    const size_t drawSize = 1024;
    std::vector<float> transformFeedbackData(drawSize);
    for (size_t i = 0; i < drawSize; i++)
    {
        transformFeedbackData[i] = static_cast<float>(i + 1);
    }

    // Initialize the buffers to zero
    size_t bufferSize = drawSize;
    std::vector<float> bufferInitialData(bufferSize, 0);

    const size_t transformFeedbackCount = 8;

    // clang-format off
    const std::string vertexShaderSource = SHADER_SOURCE
    (  #version 300 es\n
       in highp vec4 position;
       in float transformFeedbackInput;
       out float transformFeedbackOutput;
       void main(void)
       {
           gl_Position = position;
           transformFeedbackOutput = transformFeedbackInput;
       }
    );

    const std::string fragmentShaderSource = SHADER_SOURCE
    (  #version 300 es\n
       out mediump vec4 color;
       void main(void)
       {
           color = vec4(1.0, 1.0, 1.0, 1.0);
       }
    );
    // clang-format on

    std::vector<std::string> tfVaryings;
    tfVaryings.push_back("transformFeedbackOutput");

    GLuint program = CompileProgramWithTransformFeedback(vertexShaderSource, fragmentShaderSource,
                                                         tfVaryings, GL_INTERLEAVED_ATTRIBS);
    ASSERT_NE(program, 0u);
    glUseProgram(program);

    GLint positionLocation = glGetAttribLocation(program, "position");
    glDisableVertexAttribArray(positionLocation);
    glVertexAttrib4f(positionLocation, 0.0f, 0.0f, 0.0f, 1.0f);

    GLint tfInputLocation = glGetAttribLocation(program, "transformFeedbackInput");
    glEnableVertexAttribArray(tfInputLocation);
    glVertexAttribPointer(tfInputLocation, 1, GL_FLOAT, false, 0, &transformFeedbackData[0]);

    glDepthMask(GL_FALSE);
    glEnable(GL_DEPTH_TEST);
    ASSERT_GL_NO_ERROR();

    GLuint transformFeedbacks[transformFeedbackCount];
    glGenTransformFeedbacks(transformFeedbackCount, transformFeedbacks);

    GLuint buffers[transformFeedbackCount];
    glGenBuffers(transformFeedbackCount, buffers);

    for (size_t i = 0; i < transformFeedbackCount; i++)
    {
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformFeedbacks[i]);

        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffers[i]);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, bufferSize * sizeof(GLfloat),
                     &bufferInitialData[0], GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffers[i]);
        ASSERT_GL_NO_ERROR();

        glBeginTransformFeedback(GL_POINTS);

        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(drawSize));

        glPauseTransformFeedback();

        EXPECT_GL_NO_ERROR();
    }

    for (size_t i = 0; i < transformFeedbackCount; i++)
    {
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformFeedbacks[i]);
        glEndTransformFeedback();
        glDeleteTransformFeedbacks(1, &transformFeedbacks[i]);

        EXPECT_GL_NO_ERROR();
    }
}
// Test that running multiple simultaneous queries and transform feedbacks from multiple EGL
// contexts returns the correct results.  Helps expose bugs in ANGLE's virtual contexts.
TEST_P(TransformFeedbackTest, MultiContext)
{
    if (GetParam() == ES3_D3D11())
    {
        std::cout << "Test skipped because the D3D backends cannot support simultaneous transform "
                     "feedback or queries on multiple contexts yet."
                  << std::endl;
        return;
    }

#if defined(ANGLE_PLATFORM_APPLE)
    if ((isNVidia() || isAMD()) && GetParam() == ES3_OPENGL())
    {
        std::cout << "Test skipped on NVidia and AMD OpenGL on OSX." << std::endl;
        return;
    }
#endif

#if defined(ANGLE_PLATFORM_LINUX)
    if (isAMD() && GetParam() == ES3_OPENGL())
    {
        std::cout << "Test skipped on AMD OpenGL on Linux." << std::endl;
        return;
    }
#endif

    EGLint contextAttributes[] = {
        EGL_CONTEXT_MAJOR_VERSION_KHR,
        GetParam().majorVersion,
        EGL_CONTEXT_MINOR_VERSION_KHR,
        GetParam().minorVersion,
        EGL_NONE,
    };

    EGLWindow *window = getEGLWindow();

    EGLDisplay display = window->getDisplay();
    EGLConfig config   = window->getConfig();
    EGLSurface surface = window->getSurface();

    const size_t passCount = 5;
    struct ContextInfo
    {
        EGLContext context;
        GLuint program;
        GLuint query;
        GLuint buffer;
        size_t primitiveCounts[passCount];
    };
    ContextInfo contexts[32];

    const size_t maxDrawSize = 1024;

    std::vector<float> transformFeedbackData(maxDrawSize);
    for (size_t i = 0; i < maxDrawSize; i++)
    {
        transformFeedbackData[i] = static_cast<float>(i + 1);
    }

    // Initialize the buffers to zero
    size_t bufferSize = maxDrawSize * passCount;
    std::vector<float> bufferInitialData(bufferSize, 0);

    for (auto &context : contexts)
    {
        context.context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
        ASSERT_NE(context.context, EGL_NO_CONTEXT);

        eglMakeCurrent(display, surface, surface, context.context);

        // clang-format off
        const std::string vertexShaderSource = SHADER_SOURCE
        (   #version 300 es\n
            in highp vec4 position;
            in float transformFeedbackInput;
            out float transformFeedbackOutput;
            void main(void)
            {
                gl_Position = position;
                transformFeedbackOutput = transformFeedbackInput;
            }
        );

        const std::string fragmentShaderSource = SHADER_SOURCE
        (   #version 300 es\n
            out mediump vec4 color;
            void main(void)
            {
                color = vec4(1.0, 1.0, 1.0, 1.0);
            }
        );
        // clang-format on

        std::vector<std::string> tfVaryings;
        tfVaryings.push_back("transformFeedbackOutput");

        context.program = CompileProgramWithTransformFeedback(
            vertexShaderSource, fragmentShaderSource, tfVaryings, GL_INTERLEAVED_ATTRIBS);
        ASSERT_NE(context.program, 0u);
        glUseProgram(context.program);

        GLint positionLocation = glGetAttribLocation(context.program, "position");
        glDisableVertexAttribArray(positionLocation);
        glVertexAttrib4f(positionLocation, 0.0f, 0.0f, 0.0f, 1.0f);

        GLint tfInputLocation = glGetAttribLocation(context.program, "transformFeedbackInput");
        glEnableVertexAttribArray(tfInputLocation);
        glVertexAttribPointer(tfInputLocation, 1, GL_FLOAT, false, 0, &transformFeedbackData[0]);

        glDepthMask(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glGenQueriesEXT(1, &context.query);
        glBeginQueryEXT(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, context.query);

        ASSERT_GL_NO_ERROR();

        glGenBuffers(1, &context.buffer);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, context.buffer);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, bufferSize * sizeof(GLfloat),
                     &bufferInitialData[0], GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, context.buffer);

        ASSERT_GL_NO_ERROR();

        // For each pass, draw between 0 and maxDrawSize primitives
        for (auto &primCount : context.primitiveCounts)
        {
            primCount = rand() % maxDrawSize;
        }

        glBeginTransformFeedback(GL_POINTS);
    }

    for (size_t pass = 0; pass < passCount; pass++)
    {
        for (const auto &context : contexts)
        {
            eglMakeCurrent(display, surface, surface, context.context);

            glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(context.primitiveCounts[pass]));
        }
    }

    for (const auto &context : contexts)
    {
        eglMakeCurrent(display, surface, surface, context.context);

        glEndTransformFeedback();

        glEndQueryEXT(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);

        GLuint result = 0;
        glGetQueryObjectuivEXT(context.query, GL_QUERY_RESULT_EXT, &result);

        EXPECT_GL_NO_ERROR();

        size_t totalPrimCount = 0;
        for (const auto &primCount : context.primitiveCounts)
        {
            totalPrimCount += primCount;
        }
        EXPECT_EQ(static_cast<GLuint>(totalPrimCount), result);

        const float *bufferData = reinterpret_cast<float *>(glMapBufferRange(
            GL_TRANSFORM_FEEDBACK_BUFFER, 0, bufferSize * sizeof(GLfloat), GL_MAP_READ_BIT));

        size_t curBufferIndex = 0;
        for (const auto &primCount : context.primitiveCounts)
        {
            for (size_t prim = 0; prim < primCount; prim++)
            {
                EXPECT_EQ(bufferData[curBufferIndex], prim + 1);
                curBufferIndex++;
            }
        }

        while (curBufferIndex < bufferSize)
        {
            EXPECT_EQ(bufferData[curBufferIndex], 0.0f);
            curBufferIndex++;
        }

        glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
    }

    eglMakeCurrent(display, surface, surface, window->getContext());

    for (auto &context : contexts)
    {
        eglDestroyContext(display, context.context);
        context.context = EGL_NO_CONTEXT;
    }
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these tests should be run against.
ANGLE_INSTANTIATE_TEST(TransformFeedbackTest, ES3_D3D11(), ES3_OPENGL());

} // namespace
