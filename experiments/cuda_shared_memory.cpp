#include <thread>
#include <exception>
#include <iostream>
#include <string>
#include <cuda.h>
#include <cuda_gl_interop.h>
#include <cuda_runtime.h>
#include <zmq.h>
#include <string.h>
#include "cuda_shared_memory.h"
#include <map>
#include <chrono>
#include <GL/gl.h>

enum class MessageType {
    NONE = 0,

    CREATE = 1,
    CREATE_RESPONSE = 2,

    SUBMIT = 3,
    SUBMIT_RESPONSE = 4,

    FREE = 5,
    FREE_RESPONSE = 6,
};

struct Message {
    MessageType type;

    // Create:
    int width, height;

    // Create response:
    cudaIpcMemHandle_t ipc_handle;
    unsigned int server_side_id;


};

void send_message(void* socket, const Message& message) {
    zmq_msg_t msg;
    zmq_msg_init_size(&msg, sizeof(Message));
    memcpy(zmq_msg_data(&msg), &message, sizeof(Message));
    zmq_msg_send(&msg, socket, 0);
}

Message recv_message(void* socket) {
    zmq_msg_t msg;
    zmq_msg_recv(&msg, socket, 0);
    Message r = *(Message*)zmq_msg_data(&msg);
    zmq_msg_close(&msg);
    return r;
}

bool recv_message_non_blocking(void* socket, Message& r) {
    zmq_msg_t msg;
    zmq_msg_init(&msg);
    if(-1 != zmq_msg_recv(&msg, socket, ZMQ_DONTWAIT)) {
        r = *(Message*)zmq_msg_data(&msg);
        zmq_msg_close(&msg);
        return true;
    } else {
        zmq_msg_close(&msg);
        return false;
    }
}

class CUDASharedTextureClientImpl {
    void* socket_;
    unsigned int server_side_id_;
    void* device_pointer_;
    void* device_pointer_texture_;
    GLuint texture_;
    cudaGraphicsResource_t resource_;
    int width_, height_;
public:
    CUDASharedTextureClientImpl(void* zmq_context, const char* server_endpoint, int width, int height) {
        width_ = width;
        height_ = height;
        socket_ = zmq_socket(zmq_context, ZMQ_REQ);
        if(zmq_connect(socket_, server_endpoint) != 0) {
            throw std::runtime_error("zmq_connect() failed");
        }

        Message msg;
        msg.type = MessageType::CREATE;
        msg.width = width;
        msg.height = height;
        send_message(socket_, msg);
        msg = recv_message(socket_);
        server_side_id_ = msg.server_side_id;

        // Open IPC mapped CUDA memory
        cudaIpcOpenMemHandle(&device_pointer_, msg.ipc_handle, cudaIpcMemLazyEnablePeerAccess);

        glGenTextures(1, &texture_);
        glBindTexture(GL_TEXTURE_2D, texture_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);

        cudaGraphicsGLRegisterImage(&resource_, texture_, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsNone);
        std::cout << "initialized" << std::endl;
    }

    void submit() {
        auto start = std::chrono::high_resolution_clock::now();

        cudaGraphicsMapResources(1, &resource_, nullptr);
        cudaArray_t array;
        cudaGraphicsSubResourceGetMappedArray(&array, resource_, 0, 0);

        cudaMemcpy2DFromArray(device_pointer_, width_ * 4, array, 0, 0, width_, height_, cudaMemcpyDeviceToDevice);

        cudaGraphicsUnmapResources(1, &resource_, nullptr);
        std::cout << "submit end" << std::endl;
        Message msg;
        msg.type = MessageType::SUBMIT;
        msg.server_side_id = server_side_id_;
        send_message(socket_, msg);
        recv_message(socket_);

        auto finish = std::chrono::high_resolution_clock::now();
        std::cout << "Submit took " << std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count() << "ns\n";
    }

    unsigned int getOpenGLTexture() {
        return texture_;
    }

    unsigned int getID() {
        return server_side_id_;
    }

    ~CUDASharedTextureClientImpl() {
        cudaGraphicsUnregisterResource(resource_);
        cudaIpcCloseMemHandle(device_pointer_);
        zmq_close(socket_);
    }
};

CUDASharedTextureClient::CUDASharedTextureClient(void* zmq_context, const char* server_endpoint, int width, int height) {
    impl = new CUDASharedTextureClientImpl(zmq_context, server_endpoint, width, height);
}

// Get the opengl texture
unsigned int CUDASharedTextureClient::getOpenGLTexture() {
    return impl->getOpenGLTexture();
}

// Submit the texture to the server side
void CUDASharedTextureClient::submit() {
    impl->submit();
}

// Get the ID of the texture for communication with the server
unsigned int CUDASharedTextureClient::getID() {
    return impl->getID();
}

CUDASharedTextureClient::~CUDASharedTextureClient() {
    delete impl;
}

class CUDASharedTextureServerImpl {
public:

    struct ClientData {
        unsigned int id;
        cudaIpcMemHandle_t ipc_handle;
        GLuint texture;
        cudaGraphicsResource_t resource;
        void* device_pointer;
        int width;
        int height;
        size_t size;
    };

    int allocateID() {
        return current_id_++;
    }

    Message process_message(const Message& request) {
        Message response;
        response.type = MessageType::NONE;
        switch(request.type) {
            case MessageType::CREATE: {

                ClientData client;

                int id = allocateID();
                client.id = id;
                client.width = request.width;
                client.height = request.height;
                client.size = client.width * client.height * 4;

                std::cout << "Creating texture..." << std::endl;

                cudaMalloc(&client.device_pointer, client.size);
                cudaIpcGetMemHandle(&client.ipc_handle, client.device_pointer);

                glGenTextures(1, &client.texture);
                glBindTexture(GL_TEXTURE_2D, client.texture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, client.width, client.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                glBindTexture(GL_TEXTURE_2D, 0);

                cudaGraphicsGLRegisterImage(&client.resource, client.texture, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsNone);

                clients_[id] = client;

                response.type = MessageType::CREATE_RESPONSE;
                response.server_side_id = id;
                response.ipc_handle = client.ipc_handle;

            } break;
            case MessageType::SUBMIT: {

                ClientData& client = clients_[request.server_side_id];
                cudaGraphicsMapResources(1, &client.resource, nullptr);
                cudaArray_t array;
                cudaGraphicsSubResourceGetMappedArray(&array, client.resource, 0, 0);

                cudaMemcpy2DToArray(array, 0, 0, client.device_pointer, client.width * 4, client.width, client.height, cudaMemcpyDeviceToDevice);

                cudaGraphicsUnmapResources(1, &client.resource, nullptr);
                response.type = MessageType::SUBMIT_RESPONSE;

            } break;
            case MessageType::FREE: {
            } break;
        }
        return response;
    }

    // Run this every tick in the main rendering thread
    void mainThreadTick() {
        Message request;
        if(recv_message_non_blocking(socket_, request)) {
            std::cout << "tick got message" << std::endl;
            Message response = process_message(request);
            send_message(socket_, response);
        }
    }

    bool hasTexture(unsigned int id) {
        return clients_.find(id) != clients_.end();
    }

    unsigned int getTexture(unsigned int id) {
        return clients_[id].texture;
    }

    CUDASharedTextureServerImpl(void* zmq_context, const char* server_endpoint) {
        std::cout << "Shared texture server initialize" << std::endl;

        current_id_ = 1;
        socket_ = zmq_socket(zmq_context, ZMQ_REP);
        if(!socket_ || zmq_bind(socket_, server_endpoint) != 0) {
            throw std::runtime_error("zmq_bind() failed");
        }

        // Create cuda context
        unsigned int count;
        int devices[64];
        cudaGLGetDevices(&count, devices, 64, cudaGLDeviceListAll);
        for(int i = 0; i < count; i++) {
            std::cout << " CUDA Device running OpenGL: " << devices[i] << std::endl;
        }
    }

    ~CUDASharedTextureServerImpl() {
        zmq_close(socket_);
    }

private:
    void* socket_;
    unsigned int current_id_;
    std::map<unsigned int, ClientData> clients_;
};


CUDASharedTextureServer::CUDASharedTextureServer(void* zmq_context, const char* server_endpoint) {
    impl = new CUDASharedTextureServerImpl(zmq_context, server_endpoint);
}

// Get the texture from a client
unsigned int CUDASharedTextureServer::getTexture(unsigned int id) {
    return impl->getTexture(id);
}

bool CUDASharedTextureServer::hasTexture(unsigned int id) {
    return impl->hasTexture(id);
}

void CUDASharedTextureServer::mainThreadTick() {
    impl->mainThreadTick();
}

// Release all textures and shutdown
CUDASharedTextureServer::~CUDASharedTextureServer() {
    delete impl;
}