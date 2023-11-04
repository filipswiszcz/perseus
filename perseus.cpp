#include <cassert>

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION

#include <AppKit/AppKit.hpp>
#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>

#include <simd/simd.h>

static constexpr size_t INSTANCE_ROWS = 10;
static constexpr size_t INSTANCE_COLUMNS = 10;
static constexpr size_t INSTANCE_DEPTH = 10;

static constexpr size_t INSTANCES = (INSTANCE_ROWS * INSTANCE_COLUMNS * INSTANCE_DEPTH);
static constexpr size_t FRAMES = 3;

#pragma region Declaration {

    namespace math {

        constexpr simd::float3 add(const simd::float3& a, const simd::float3& b);
        constexpr simd_float4x4 identity();

        simd::float4x4 perspective();

        simd::float4x4 rotateX(float radiansAngle);

        simd::float4x4 rotateY(float radiansAngle);

        simd::float4x4 rotateZ(float radiansAngle);

        simd::float4x4 translate(const simd::float3& vec);

        simd::float4x4 scale(const simd::float3& vec);

        simd::float3x3 discard(const simd::float4x4& matr);

    }

    class Render {

        public:

            Render(MTL::Device* device);

            ~Render();

            void buildShaders();

            void buildDepthStencilStates();

            void buildBuffers();

            void draw(MTK::View* view);

        private:

            MTL::Device* _device;
            MTL::CommandQueue* _commandQueue;
            MTL::Library* _shaderLibrary;
            MTL::RenderPipelineState* _pipeState;
            MTL::DepthStencilState* _depthStencilState;
            MTL::Buffer* _vertexDataBuff;
            MTL::Buffer* _instanceDataBuff[FRAMES];
            MTL::Buffer* _cameraDataBuff[FRAMES];
            MTL::Buffer* _indexBuff;

            float _angle;
            int _frame;

            dispatch_semaphore_t _semaphore;

            static const int FRAMES;
            
    };

    class CoreViewDelegate : public MTK::ViewDelegate {

        public:

            CoreViewDelegate(MTL::Device* device);

            virtual ~CoreViewDelegate() override;

            virtual void drawInMTKView(MTK::View* view) override;

        private:

            Render* _render;

    };

    class CoreApplicationDelegate: public NS::ApplicationDelegate {

        public:

            ~CoreApplicationDelegate();

            NS::Menu* createMenuBar();

            virtual void applicationWillFinishLaunching(NS::Notification* notification) override;

            virtual void applicationDidFinishLaunching(NS::Notification* notification) override;

            virtual bool applicationShouldTerminateAfterLastWindowClosed(NS::Application* application) override;

        private:

            NS::Window* _window;
            MTK::View* _view;
            MTL::Device* _device;
            CoreViewDelegate* _coreViewDelegate = nullptr;

    };

#pragma endregion Declaration }

int main() {

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc() -> init();

    CoreApplicationDelegate coreApp;

    NS::Application* sharedApp = NS::Application::sharedApplication();

    sharedApp -> setDelegate(&coreApp);
    sharedApp -> run();

    pool -> release();

    return 0;
}

#pragma mark - CoreApplicationDelegate
#pragma region CoreApplicationDelegate {

    CoreApplicationDelegate::~CoreApplicationDelegate() {

        _view -> release();
        _window -> release();
        _device -> release();

        delete _coreViewDelegate;   
    }

    NS::Menu* CoreApplicationDelegate::createMenuBar() {

        using NS::UTF8StringEncoding;

        NS::Menu* coreMenu = NS::Menu::alloc() -> init();
        NS::MenuItem* appMenuItem = NS::MenuItem::alloc() -> init();
        NS::Menu* appMenu = NS::Menu::alloc() -> init(NS::String::string("Appname", UTF8StringEncoding));

        NS::String* appName = NS::RunningApplication::currentApplication() -> localizedName();
        NS::String* quitItemName = NS::String::string("Quit", UTF8StringEncoding) -> stringByAppendingString(appName);
        SEL quit = NS::MenuItem::registerActionCallback("appQuit", [](void*, SEL, const NS::Object* application) {
            
            auto app = NS::Application::sharedApplication();

            app -> terminate(application);
        });

        NS::MenuItem* appQuitItem = appMenu -> addItem(quitItemName, quit, NS::String::string("q", UTF8StringEncoding));

        appQuitItem -> setKeyEquivalentModifierMask(NS::EventModifierFlagCommand);

        appMenuItem -> setSubmenu(appMenu);

        NS::MenuItem* windowMenuItem = NS::MenuItem::alloc() -> init();
        NS::Menu* windowMenu = NS::Menu::alloc() -> init(NS::String::string("Window", UTF8StringEncoding));

        SEL closeWindow = NS::MenuItem::registerActionCallback("windowClose", [](void*, SEL, const NS::Object*) {

            auto app = NS::Application::sharedApplication();

            app -> windows() -> object<NS::Window>(0) -> close();
        });

        NS::MenuItem* closeWindowItem = windowMenu -> addItem(NS::String::string("Close Window", UTF8StringEncoding), closeWindow, NS::String::string("w", UTF8StringEncoding));
        
        closeWindowItem -> setKeyEquivalentModifierMask(NS::EventModifierFlagCommand);

        windowMenuItem -> setSubmenu(windowMenu);

        coreMenu -> addItem(appMenuItem);
        coreMenu -> addItem(windowMenuItem);

        appMenuItem -> release();
        windowMenuItem -> release();
        appMenu -> release();
        windowMenu -> release();

        return coreMenu -> autorelease();
    }

    void CoreApplicationDelegate::applicationWillFinishLaunching(NS::Notification* notification) {

        NS::Menu* menu = createMenuBar();
        NS::Application* app = reinterpret_cast<NS::Application*>(notification -> object());

        app -> setMainMenu(menu);
        app -> setActivationPolicy(NS::ActivationPolicy::ActivationPolicyRegular);
    }

    void CoreApplicationDelegate::applicationDidFinishLaunching(NS::Notification* notification) {

        CGRect frame = (CGRect) {
            {100.0, 100.0},
            {1024.0, 1024.0}
        };

        _window = NS::Window::alloc() -> init(
            frame,
            NS::WindowStyleMaskClosable|NS::WindowStyleMaskTitled,
            NS::BackingStoreBuffered,
            false
        );

        _device = MTL::CreateSystemDefaultDevice();

        _view = MTK::View::alloc() -> init(frame, _device);
        _view -> setColorPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);
        _view -> setClearColor(MTL::ClearColor::Make(0.0, 0.0, 0.0, 1.0));
        _view -> setDepthStencilPixelFormat(MTL::PixelFormat::PixelFormatDepth16Unorm);
        _view -> setClearDepth(1.0f);

        _coreViewDelegate = new CoreViewDelegate(_device);

        _view -> setDelegate(_coreViewDelegate);

        _window -> setContentView(_view);
        _window -> setTitle(NS::String::string("Powered by Perseus", NS::StringEncoding::UTF8StringEncoding));
        _window -> makeKeyAndOrderFront(nullptr);

        NS::Application* app = reinterpret_cast<NS::Application*>(notification -> object());
        
        app -> activateIgnoringOtherApps(true);
    }

    bool CoreApplicationDelegate::applicationShouldTerminateAfterLastWindowClosed(NS::Application* application) {
        return true;
    }

#pragma endregion CoreApplicationDelegate }

#pragma mark - CoreViewDelegate
#pragma region CoreViewDelegate {

    CoreViewDelegate::CoreViewDelegate(MTL::Device* device) : MTK::ViewDelegate(), _render(new Render(device)) {}

    CoreViewDelegate::~CoreViewDelegate() {
        delete _render;
    }

    void CoreViewDelegate::drawInMTKView(MTK::View* view) {
        _render -> draw(view);
    }

#pragma endregion CoreViewDelegate }

#pragma mark - Math

    namespace math {

        constexpr simd::float3 add(const simd::float3& a, const simd::float3& b) {
            return {
                a.x + b.x,
                a.y + b.y,
                a.z + b.z
            };
        }

        constexpr simd_float4x4 identity() {
            return (simd_float4x4) {
                (simd::float4) {1.f, 0.f, 0.f, 0.f},
                (simd::float4) {0.f, 1.f, 0.f, 0.f},
                (simd::float4) {0.f, 0.f, 1.f, 0.f},
                (simd::float4) {0.f, 0.f, 0.f, 1.f}
            };
        }

        simd::float4x4 perspective(float fov, float aspect, float nearZ, float farZ) {

            float ys = 1.f / tanf(fov * 0.5f);
            float xs = ys / aspect;
            float zs = farZ / (nearZ - farZ);

            return simd_matrix_from_rows(
                (simd::float4) {xs, 0.0f, 0.0f, 0.0f},
                (simd::float4) {0.0f, ys, 0.0f, 0.0f},
                (simd::float4) {0.0f, 0.0f, zs, nearZ * zs},
                (simd::float4) {0, 0, -1, 0}
            );
        }

        simd::float4x4 rotateX(float radiansAngle) {
            return simd_matrix_from_rows(
                (simd::float4) {1.0f, 0.0f, 0.0f, 0.0f},
                (simd::float4) {0.0f, cosf(radiansAngle), sinf(radiansAngle), 0.0f},
                (simd::float4) {0.0f, -sinf(radiansAngle), cosf(radiansAngle), 0.0f},
                (simd::float4) {0.0f, 0.0f, 0.0f, 1.0f}
            );
        }

        simd::float4x4 rotateY(float radiansAngle) {
            return simd_matrix_from_rows(
                (simd::float4) {cosf(radiansAngle), 0.0f, sinf(radiansAngle), 0.0f},
                (simd::float4) {0.0f, 1.0f, 0.0f, 0.0f},
                (simd::float4) {-sinf(radiansAngle), 0.0f, cosf(radiansAngle), 0.0f},
                (simd::float4) {0.0f, 0.0f, 0.0f, 1.0f}
            );
        }

        simd::float4x4 rotateZ(float radiansAngle) {
            return simd_matrix_from_rows(
                (simd::float4) {cosf(radiansAngle), sinf(radiansAngle), 0.0f, 0.0f},
                (simd::float4) {-sinf(radiansAngle), cosf(radiansAngle), 0.0f, 0.0f},
                (simd::float4) {0.0f, 0.0f, 1.0f, 0.0f},
                (simd::float4) {0.0f, 0.0f, 0.0f, 1.0f}
            );
        }

        simd::float4x4 translate(const simd::float3& vec) {
            return simd_matrix(
                (simd::float4) {1.0f, 0.0f, 0.0f, 0.0f},
                (simd::float4) {0.0f, 1.0f, 0.0f, 0.0f},
                (simd::float4) {0.0f, 0.0f, 1.0f, 0.0f},
                (simd::float4) {vec.x, vec.y, vec.z, 1.0f}
            );
        }

        simd::float4x4 scale(const simd::float3& vec) {
            return simd_matrix(
                (simd::float4) {vec.x, 0, 0, 0},
                (simd::float4) {0, vec.y, 0, 0},
                (simd::float4) {0, 0, vec.z, 0},
                (simd::float4) {0, 0, 0, 1.0}
            );
        }

        simd::float3x3 discard(const simd::float4x4& matr) {
            return simd_matrix(
                matr.columns[0].xyz,
                matr.columns[1].xyz,
                matr.columns[2].xyz
            );
        }

    }

#pragma mark - Render
#pragma region Render {

    const int Render::FRAMES = 3;

    Render::Render(MTL::Device* device) : _device(device -> retain()), _angle(0.f), _frame(0) {
        _commandQueue = _device -> newCommandQueue();

        buildShaders();
        buildDepthStencilStates();
        buildBuffers();

        _semaphore = dispatch_semaphore_create(Render::FRAMES);
    }

    Render::~Render() {
        _shaderLibrary -> release();
        _depthStencilState -> release();
        _vertexDataBuff -> release();

        for (int i = 0; i < FRAMES; i++) {
            _instanceDataBuff[i] -> release();
        }

        for (int i = 0; i < FRAMES; i++) {
            _cameraDataBuff[i] -> release();
        }

        _indexBuff -> release();
        _pipeState -> release();
        _commandQueue -> release();
        _device -> release();
    }

    namespace shader {

        struct VertexData {

            simd::float3 position;
            simd::float3 normal;

        };

        struct InstanceData {

            simd::float4x4 instanceTransform;
            simd::float3x3 instanceNormalTransform;
            simd::float4 instanceColor;

        };

        struct CameraData {

            simd::float4x4 perspectiveTransform;
            simd::float4x4 worldTransform;
            simd::float3x3 worldNormalTransform;

        };

    }

    void Render::buildShaders() {

        using NS::UTF8StringEncoding;

        const char* src = R"(
            #include <metal_stdlib>

            using namespace metal;

            struct v2f {

                float4 position [[position]];
                float3 normal;
                half3 color;

            };

            struct VertexData {

                float3 position;
                float3 normal;

            };

            struct InstanceData {

                float4x4 instanceTransform;
                float3x3 instanceNormalTransform;
                float4 instanceColor;

            };

            struct CameraData {

                float4x4 perspectiveTransform;
                float4x4 worldTransform;
                float3x3 worldNormalTransform;

            };

            v2f vertex vertexCore(uint vertexId [[vertex_id]],
                uint instanceId [[instance_id]],
                device const VertexData* vertexData [[buffer(0)]],
                device const InstanceData* instanceData [[buffer(1)]],
                device const CameraData& cameraData [[buffer(2)]]) {

                    v2f out;

                    float4 pos = float4(vertexData[vertexId].position, 1.0);

                    pos = instanceData[instanceId].instanceTransform * pos;
                    pos = cameraData.perspectiveTransform * cameraData.worldTransform * pos;

                    float3 norm = instanceData[instanceId].instanceNormalTransform * vertexData[vertexId].normal;
                    
                    norm = cameraData.worldNormalTransform * norm;

                    out.position = pos;
                    out.normal = norm;
                    out.color = half3(instanceData[instanceId].instanceColor.rgb);

                    return out;
                }

            half4 fragment fragmentCore(v2f in [[stage_in]]) {

                float3 l = normalize(float3(1.0, 1.0, 0.8));
                float3 n = normalize(in.normal);

                float ndotl = saturate(dot(n, l));

                return half4(in.color * 0.1 + in.color * ndotl, 1.0);
            }

        )";

        NS::Error* err = nullptr;
        MTL::Library* lib = _device -> newLibrary(NS::String::string(src, UTF8StringEncoding), nullptr, &err);

        if (!lib) {
            __builtin_printf("%s", err -> localizedDescription() -> utf8String());
            assert(false);
        }

        MTL::Function* vFn = lib -> newFunction(NS::String::string("vertexCore", UTF8StringEncoding));
        MTL::Function* fFn = lib -> newFunction(NS::String::string("fragmentCore", UTF8StringEncoding));

        MTL::RenderPipelineDescriptor* desc = MTL::RenderPipelineDescriptor::alloc() -> init();

        desc -> setVertexFunction(vFn);
        desc -> setFragmentFunction(fFn);
        desc -> colorAttachments() -> object(0) -> setPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);
        desc -> setDepthAttachmentPixelFormat(MTL::PixelFormat::PixelFormatDepth16Unorm);

        _pipeState = _device -> newRenderPipelineState(desc, &err);

        if (!_pipeState) {
            __builtin_printf("%s", err -> localizedDescription() -> utf8String());
            assert(false);
        }

        vFn -> release();
        fFn -> release();
        
        desc -> release();
        
        _shaderLibrary = lib;

    }

    void Render::buildDepthStencilStates() {
        
        MTL::DepthStencilDescriptor* desc = MTL::DepthStencilDescriptor::alloc() -> init();

        desc -> setDepthCompareFunction(MTL::CompareFunction::CompareFunctionLess);
        desc -> setDepthWriteEnabled(true);

        _depthStencilState = _device -> newDepthStencilState(desc);

        desc -> release();
    }

    void Render::buildBuffers() {

        using simd::float3;

        const float s = 0.5f;

        shader::VertexData verts[] = {
            {{-s, -s, +s}, {0.f, 0.f, 1.f}},
            {{+s, -s, +s}, {0.f, 0.f, 1.f}},
            {{+s, +s, +s}, {0.f, 0.f, 1.f}},
            {{-s, +s, +s}, {0.f, 0.f, 1.f}},

            {{+s, -s, +s}, {1.f, 0.f, 0.f}},
            {{+s, -s, -s}, {1.f, 0.f, 0.f}},
            {{+s, +s, -s}, {1.f, 0.f, 0.f}},
            {{+s, +s, +s}, {1.f, 0.f, 0.f}},

            {{+s, -s, -s}, {0.f, 0.f, -1.f}},
            {{-s, -s, -s}, {0.f, 0.f, -1.f}},
            {{-s, +s, -s}, {0.f, 0.f, -1.f}},
            {{+s, +s, -s}, {0.f, 0.f, -1.f}},

            {{-s, -s, -s}, {-1.f, 0.f, 0.f}},
            {{-s, -s, +s}, {-1.f, 0.f, 0.f}},
            {{-s, +s, +s}, {-1.f, 0.f, 0.f}},
            {{-s, +s, -s}, {-1.f, 0.f, 0.f}},

            {{-s, +s, +s}, {0.f, 1.f, 0.f}},
            {{+s, +s, +s}, {0.f, 1.f, 0.f}},
            {{+s, +s, -s}, {0.f, 1.f, 0.f}},
            {{-s, +s, -s}, {0.f, 1.f, 0.f}},

            {{-s, -s, -s}, {0.f, -1.f, 0.f}},
            {{+s, -s, -s}, {0.f, -1.f, 0.f}},
            {{+s, -s, +s}, {0.f, -1.f, 0.f}},
            {{-s, -s, +s}, {0.f, -1.f, 0.f}}
        };

        uint16_t indices[] = {
            0, 1, 2,
            2, 3, 0,

            4, 5, 6,
            6, 7, 4,

            8, 9, 10,
            10, 11, 8,

            12, 13, 14,
            14, 15, 12,

            16, 17, 18,
            18, 19, 16,

            20, 21, 22,
            22, 23, 20
        };

        const size_t vertexDataSize = sizeof(verts);
        const size_t indexDataSize = sizeof(indices);

        MTL::Buffer* vertBuff = _device -> newBuffer(vertexDataSize, MTL::ResourceStorageModeManaged);
        MTL::Buffer* indBuff = _device -> newBuffer(indexDataSize, MTL::ResourceStorageModeManaged);

        _vertexDataBuff = vertBuff;
        _indexBuff = indBuff;

        memcpy(_vertexDataBuff -> contents(), verts, vertexDataSize);
        memcpy(_indexBuff -> contents(), indices, indexDataSize);

        _vertexDataBuff -> didModifyRange(NS::Range::Make(0, _vertexDataBuff -> length()));
        _indexBuff -> didModifyRange(NS::Range::Make(0, _indexBuff -> length()));

        const size_t instanceDataSize = FRAMES * INSTANCES * sizeof(shader::InstanceData);

        for (size_t i = 0; i < FRAMES; i++) {
            _instanceDataBuff[i] = _device -> newBuffer(instanceDataSize, MTL::ResourceStorageModeManaged);
        }

        const size_t cameraDataSize = FRAMES * sizeof(shader::CameraData);

        for (size_t i = 0; i < FRAMES; i++) {
            _cameraDataBuff[i] = _device -> newBuffer(cameraDataSize, MTL::ResourceStorageModeManaged);
        }
    }

    void Render::draw(MTK::View* view) {

        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc() -> init();

        _frame = (_frame + 1) % Render::FRAMES;

        MTL::Buffer* insBuff = _instanceDataBuff[_frame];
        MTL::CommandBuffer* cmdBuff = _commandQueue -> commandBuffer();

        dispatch_semaphore_wait(_semaphore, DISPATCH_TIME_FOREVER);
        Render* render = this;

        cmdBuff -> addCompletedHandler([render](MTL::CommandBuffer* cmdBuff) {
            dispatch_semaphore_signal(render -> _semaphore);
        });

        _angle += 0.002f;
        const float scl = 0.2f;

        shader::InstanceData* insData = reinterpret_cast<shader::InstanceData*>(insBuff -> contents());

        simd::float3 objectPos = {0.f, 0.f, -10.f};

        simd::float4x4 trans = math::translate(objectPos);
        simd::float4x4 rotY = math::rotateY(-_angle);
        simd::float4x4 rotX = math::rotateX(_angle * 0.5);
        simd::float4x4 inverTrans = math::translate({
            -objectPos.x, -objectPos.y, -objectPos.z
        });
        simd::float4x4 fullRot = trans * rotY * rotX * inverTrans;

        size_t xI = 0;
        size_t yI = 0;
        size_t zI = 0;

        for (size_t i = 0; i < INSTANCES; i++) {

            if (xI == INSTANCE_ROWS) {

                xI = 0;
                yI += 1;

            }

            if (yI == INSTANCE_ROWS) {

                yI = 0;
                zI += 1;

            }

            simd::float4x4 scale = math::scale((simd::float3) {scl, scl, scl});
            simd::float4x4 rotZ = math::rotateZ(_angle * sinf((float) xI));
            simd::float4x4 rotY = math::rotateY(_angle * cosf((float) yI));

            float x = ((float) xI - (float) INSTANCE_ROWS / 2.f) * (2.f * scl) + scl;
            float y = ((float) yI - (float) INSTANCE_COLUMNS / 2.f) * (2.f * scl) + scl;
            float z = ((float) zI - (float) INSTANCE_DEPTH / 2.f) * (2.f * scl);

            simd::float4x4 translate = math::translate(math::add(objectPos, {x, y, z}));

            insData[i].instanceTransform = fullRot * translate * rotY * rotZ * scale;
            insData[i].instanceNormalTransform = math::discard(insData[i].instanceTransform);

            float divIns = i / (float) INSTANCES;
            float r = divIns;
            float g = 1.0f - r;
            float b = sinf(M_PI * 2.0f * divIns);
            
            insData[i].instanceColor = (simd::float4) {r, g, b, 1.0f};

            xI += 1;
        }
        
        insBuff -> didModifyRange(NS::Range::Make(0, insBuff -> length()));

        MTL::Buffer* camBuff = _cameraDataBuff[_frame];

        shader::CameraData* cameraData = reinterpret_cast<shader::CameraData*>(camBuff -> contents());

        cameraData -> perspectiveTransform = math::perspective(45.f * M_PI / 180.f, 1.f, 0.03f, 500.0f);
        cameraData -> worldTransform = math::identity();
        cameraData -> worldNormalTransform = math::discard(cameraData -> worldTransform);
        
        camBuff -> didModifyRange(NS::Range::Make(0, sizeof(shader::CameraData)));

        MTL::RenderPassDescriptor* passDesc = view -> currentRenderPassDescriptor();
        MTL::RenderCommandEncoder* cmdEncoder = cmdBuff -> renderCommandEncoder(passDesc);

        cmdEncoder -> setRenderPipelineState(_pipeState);
        cmdEncoder -> setDepthStencilState(_depthStencilState);
        cmdEncoder -> setVertexBuffer(_vertexDataBuff, 0, 0);
        cmdEncoder -> setVertexBuffer(insBuff, 0, 1);
        cmdEncoder -> setVertexBuffer(camBuff, 0, 2);
        cmdEncoder -> setCullMode(MTL::CullModeBack);
        cmdEncoder -> setFrontFacingWinding(MTL::Winding::WindingCounterClockwise);
        cmdEncoder -> drawIndexedPrimitives(
            MTL::PrimitiveType::PrimitiveTypeTriangle,
            6 * 6,
            MTL::IndexType::IndexTypeUInt16,
            _indexBuff,
            0,
            INSTANCES
        );
        cmdEncoder -> endEncoding();

        cmdBuff -> presentDrawable(view -> currentDrawable());
        cmdBuff -> commit();

        pool -> release();
    }

#pragma endregion Render }
