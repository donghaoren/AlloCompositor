class CUDASharedTextureClientImpl;
class CUDASharedTextureServerImpl;

class CUDASharedTextureClient {
    CUDASharedTextureClientImpl* impl;
public:
    // Connect to server and request a shared texture
    CUDASharedTextureClient(void* zmq_context, const char* server_endpoint, int width, int height);

    // Get the opengl texture
    unsigned int getOpenGLTexture();

    // Submit the texture to the server side
    void submit();

    // Get the ID of the texture for communication with the server
    unsigned int getID();

    ~CUDASharedTextureClient();
};

class CUDASharedTextureServer {
    CUDASharedTextureServerImpl* impl;
public:
    CUDASharedTextureServer(void* zmq_context, const char* server_endpoint);

    // Get the texture from a client
    unsigned int getTexture(unsigned int id);
    bool hasTexture(unsigned int id);

    void mainThreadTick();

    // Release all textures and shutdown
    ~CUDASharedTextureServer();
};
