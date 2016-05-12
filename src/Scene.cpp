#include <climits>
#include <glm/glm.hpp>
#include <string>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "Scene.hpp"
#include "Main.hpp"

using namespace gmu;

using glm::u8vec3;
using glm::vec3;

const u8vec3 Scene::colors[] = {
    u8vec3(170,153, 57),
    u8vec3(130,110,  0),
    u8vec3(150,130, 21),
    u8vec3(190,177,101),
    u8vec3(210,202,153),

    u8vec3( 94,151, 50),
    u8vec3( 50,115,  0),
    u8vec3( 68,133, 18),
    u8vec3(124,169, 89),
    u8vec3(158,187,135),

    u8vec3(170, 99, 57),
    u8vec3(130, 48,  0),
    u8vec3(150, 69, 21),
    u8vec3(190,134,101),
    u8vec3(210,174,153),
    u8vec3(128,128,128), // Static box color
};

Scene::Scene(q3OpenCLDevice device) : vao(0), vbo(0), ebo(0),
    scene(1/60.0f, device, q3Vec3(0.0, -9.8, 0.0), 20), loop(0) {

    prepareScene();
}

Scene::Scene(Camera *_camera, q3OpenCLDevice device) : Scene(device) {

    camera = _camera;
    polygonMode = GL_FILL;

    string vertexShaderFile("./shaders/shader.vert");
    string fragmentShaderFile("./shaders/shader.frag");

    registerRenderer(camera);

    camera->setPosition(vec3(43.601841, 65.683090, -76.148041));
    camera->setRotation(vec2(0.272002, -0.476000));

    renderProgram.setVertexShaderFromFile(vertexShaderFile);
    renderProgram.setFragmenShaderFromFile(fragmentShaderFile);

    GLuint program = renderProgram.getProgram();
    if (program == 0) {
        throw string("Could not compile render program.");
    }

    uProjection = glGetUniformLocation(program, "projection");
    uView = glGetUniformLocation(program, "view");
    uModel = glGetUniformLocation(program, "model");

    uSunPosition = glGetUniformLocation(program, "sunPosition");
    uSunColor = glGetUniformLocation(program, "sunColor");

    aPosition = glGetAttribLocation(program, "position");
    aNormal = glGetAttribLocation(program, "normal");
    aColor = glGetAttribLocation(program, "color");


    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glEnableVertexAttribArray(aPosition);
    glVertexAttribPointer(aPosition, 3, GL_FLOAT, GL_FALSE, sizeof (Vertex), (GLvoid*) offsetof(Vertex, position));

    glEnableVertexAttribArray(aNormal);
    glVertexAttribPointer(aNormal, 3, GL_FLOAT, GL_FALSE, sizeof (Vertex), (GLvoid*) offsetof(Vertex, normal));

    glEnableVertexAttribArray(aColor);
    glVertexAttribPointer(aColor, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof (Vertex), (GLvoid*) offsetof(Vertex, color));

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * boxes.size() * 6 * 4, NULL, GL_STREAM_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * boxes.size() * 6 * 5, NULL, GL_STREAM_DRAW);

    glBindVertexArray(0);
}

Scene::~Scene() {

    if(ebo != 0) {
        glDeleteBuffers(1, &vbo);
        glDeleteBuffers(1, &ebo);

        glDeleteVertexArrays(1, &vao);
    }
}

void Scene::prepareScene() {
    const q3BoxRef *box;
    int numColors = (sizeof(Scene::colors) / sizeof(u8vec3)) - 1;
    int colorIndex = 0;

    boxes.clear();
    boxes.reserve(16*16*10 + 1);

    q3BodyDef bodyDef;

    q3BodyRef *b = scene.CreateBody(bodyDef);

    q3BoxDef boxDef;
    boxDef.SetRestitution(0.0f);
    q3Transform tx;
    q3Identity(tx);
    boxDef.Set(tx, q3Vec3(50.0f, 1.0f, 50.0f));
    box = b->AddBox(boxDef);
    box->SetUserdata((void*)(Scene::colors + numColors));

    boxes.push_back(box);

    bodyDef.bodyType = eDynamicBody;
    boxDef.Set( tx, q3Vec3( 1.0f, 1.0f, 1.0f ) );

    int width = 8, height = 16, depth = 8;

    for ( int i = 0; i < height; ++i )
    {
        for ( int j = -width/2; j < width/2; ++j )
        {
            for ( int k = -depth/2; k < depth/2; ++k )
            {
                bodyDef.position.Set( 1.1f * j, 1.1f * i + 5.0f, 1.1f * k );
                b = scene.CreateBody( bodyDef );
                box = b->AddBox( boxDef );

                box->SetUserdata((void*)(Scene::colors + colorIndex));

                boxes.push_back(box);

                colorIndex = (colorIndex + 59) % numColors;
            }
        }
    }
}

mat4 Scene::getProjectionMatrix() {

    vec2 windowSize = camera->getWindowSize();
    float fov = radians(75.0);
    float aspect = windowSize.x / windowSize.y;
    float near = 0.001;
    float far = 1e5;

    return glm::perspective(fov, aspect, near, far);
}

mat4 Scene::getViewMatrix() {

    vec3 cameraPosition = camera->getPosition();
    vec3 atPosition = cameraPosition + camera->getViewVector();

    return glm::lookAt(cameraPosition, atPosition, vec3(0, 1, 0));
}

void Scene::render() {

    runRenderers();

    glUseProgram(renderProgram.getProgram());

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mat4 viewMat = getViewMatrix();
    mat4 projMat = getProjectionMatrix();

//    vec3 camPos = camera->getPosition();
//    vec3 atPosition = camera->getViewVector();
//    std::cout << "Eye: (" << camPos.x << ", " << camPos.y << ", " << camPos.z << ")" << std::endl;
//    std::cout << "At: (" << atPosition.x << ", " << atPosition.y << ", " << atPosition.z << ")" << std::endl;

    glUniformMatrix4fv(uView, 1, GL_FALSE, (GLfloat*) & viewMat);
    glUniformMatrix4fv(uProjection, 1, GL_FALSE, (GLfloat*) & projMat);

    glBindVertexArray(vao);

    glPolygonMode(GL_FRONT, polygonMode);

    Vertex *vert = (Vertex*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    GLuint *elem = (GLuint*)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);

    unsigned index = 0;
    for(auto b : boxes) {
        prepareBuffers(index, b, vert, elem);
    }

    glUnmapBuffer(GL_ARRAY_BUFFER);
    glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    glDrawElements(GL_TRIANGLE_STRIP, boxes.size() * 6 * 5, GL_UNSIGNED_INT, 0);
    glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Scene::prepareBuffers(unsigned &index, const q3BoxRef* box, Vertex* vert, GLuint* elem) {
    Vertex *boxVerticies = vert + (index * 6 * 4);
    GLuint *boxElements = elem + (index * 6 * 5);
    GLuint eOffset = index * 6 * 4;
    index++;

    q3Body *body = box->body();

    q3Transform tx = body->GetTransform();
    q3Transform local = box->box()->local;
    q3Vec3 e = box->box()->e;

    q3Transform world = q3Mul( tx, local );

    q3Vec3 vertices[ 8 ] = {
        q3Vec3( -e.x, -e.y, -e.z ),
        q3Vec3( -e.x, -e.y,  e.z ),
        q3Vec3( -e.x,  e.y, -e.z ),
        q3Vec3( -e.x,  e.y,  e.z ),
        q3Vec3(  e.x, -e.y, -e.z ),
        q3Vec3(  e.x, -e.y,  e.z ),
        q3Vec3(  e.x,  e.y, -e.z ),
        q3Vec3(  e.x,  e.y,  e.z )
    };

    unsigned elements[] = {
        0, 1, 2, 3,
        1, 5, 3, 7,
        5, 4, 7, 6,
        4, 0, 6, 2,
        2, 3, 6, 7,
        4, 5, 0, 1,
    };

    q3Vec3 face[4];
    q3Vec3 *a = face + 0;
    q3Vec3 *b = face + 1;
    q3Vec3 *c = face + 2;

    for (int i = 0; i < 24; i += 4)
    {
        for(int j = 0; j < 4; j++) {
            face[j] = q3Mul( world, vertices[ elements[ i + j ] ] );
        }

        q3Vec3 n = q3Normalize( q3Cross( *b - *a, *c - *a ) );

        for(int j = 0; j < 4; j++) {
            Vertex *v = boxVerticies + i + j;
            q3Vec3 *q = face + j;

            v->position = vec3(q->x, q->y, q->z);
            v->normal = vec3(n.x, n.y, n.z);
            u8vec3 *c = (u8vec3*)box->GetUserdata();
            v->color = *c;

            *boxElements = eOffset + i + j;
            boxElements++;
        }

        *boxElements = UINT32_MAX;
        boxElements++;
    }
}


void Scene::step(float time, float delta) {
    std::cout << "Loop: " << loop << std::endl;
    // printf("Delta: %f", scene.m_dt);
    scene.Step();

    ++loop;
}

IEventListener::EventResponse Scene::onEvent(SDL_Event* evt) {

    if (evt->type == SDL_KEYDOWN) {
        SDL_KeyboardEvent *e = &evt->key;

        if (e->keysym.sym == SDLK_p) {
            switch (polygonMode) {
                case GL_FILL:
                    polygonMode = GL_LINE;
                    break;
                case GL_LINE:
                    polygonMode = GL_POINT;
                    break;
                case GL_POINT:
                    polygonMode = GL_FILL;
                    break;
            }
            return EVT_PROCESSED;
        }
    }

    return EVT_IGNORED;
}
