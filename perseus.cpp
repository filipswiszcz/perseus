#include <cassert>

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION

#include <AppKit/AppKit.hpp>
#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>

#include <simd/simd.h>

static constexpr size_t instances = 32;
static constexpr size_t maxFrames = 3;

#pragma region Declaration {

    class Render {

        public:

            Render(MTL::Device* device);

            ~Render();

            void buildShaders();

            void buildBuffers();

            void draw(MTK::View* view);

        private:

            MTL::Device* _device;
            MTL::CommandQueue* _commandQueue;
            MTL::Library* _shaderLibrary;
            MTL::RenderPipelineState* _pipeState;
            MTL::Buffer* _vertexDataBuff;
            MTL::Buffer* _instanceDataBuff[maxFrames];
            MTL::Buffer* _indexBuff;

            float _angle;
            int _frame;

            dispatch_semaphore_t _semaphore;

            static const int maxFrames;
            
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
        _view -> setClearColor(MTL::ClearColor::Make(0.0, 0.0, 0.0, 0.8));

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

#pragma mark - Render
#pragma region Render {

    const int Render::maxFrames = 3;

    Render::Render(MTL::Device* device) : _device(device -> retain()), _angle(0.f), _frame(0) {
        _commandQueue = _device -> newCommandQueue();

        buildShaders();
        buildBuffers();

        _semaphore = dispatch_semaphore_create(Render::maxFrames);
    }

    Render::~Render() {
        _shaderLibrary -> release();
        _vertexDataBuff -> release();

        for (int i = 0; i < maxFrames; i++) {
            _instanceDataBuff[i] -> release();
        }

        _indexBuff -> release();
        _pipeState -> release();
        _commandQueue -> release();
        _device -> release();
    }

    namespace shader {

        struct InstanceCoreData {

            simd::float4x4 instanceTransform;
            simd::float4 instanceColor;

        };

    }

    void Render::buildShaders() {

        using NS::UTF8StringEncoding;

        const char* src = R"(
            #include <metal_stdlib>

            using namespace metal;

            struct v2f {

                float4 position [[position]];
                half3 color;

            };

            struct VertexCoreData {

                float3 position;

            };

            struct InstanceCoreData {

                float4x4 instanceTransform;
                float4 instanceColor;

            };

            v2f vertex vertexCore(uint vertexId [[vertex_id]],
                uint instanceId [[instance_id]],
                device const VertexCoreData* vertexCoreData [[buffer(0)]],
                device const InstanceCoreData* instanceCoreData [[buffer(1)]]) {

                    v2f out;

                    float4 pos = float4(vertexCoreData[vertexId].position, 1.0);

                    out.position = instanceCoreData[instanceId].instanceTransform * pos;
                    out.color = half3(instanceCoreData[instanceId].instanceColor.rgb);

                    return out;
                }

            half4 fragment fragmentCore(v2f in [[stage_in]]) {
                return half4(in.color, 1.0);
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

    void Render::buildBuffers() {

        const float s = 0.5f;

        simd::float3 verts[] = {
            {-s, -s, +s},
            {+s, -s, +s},
            {+s, +s, +s},
            {-s, +s, +s}
        };

        uint16_t indices[] = {
            0, 1, 2,
            2, 3, 0,
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

        const size_t instanceDataSize = maxFrames * instances * sizeof(shader::InstanceCoreData);

        for (size_t i = 0; i < maxFrames; i++) {
            _instanceDataBuff[i] = _device -> newBuffer(instanceDataSize, MTL::ResourceStorageModeManaged);
        }
    }

    void Render::draw(MTK::View* view) {

        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc() -> init();

        _frame = (_frame + 1) % Render::maxFrames;

        MTL::Buffer* insBuff = _instanceDataBuff[_frame];
        MTL::CommandBuffer* cmdBuff = _commandQueue -> commandBuffer();

        dispatch_semaphore_wait(_semaphore, DISPATCH_TIME_FOREVER);
        Render* render = this;

        cmdBuff -> addCompletedHandler([render](MTL::CommandBuffer* cmdBuff) {
            dispatch_semaphore_signal(render -> _semaphore);
        });

        _angle += 0.01f;
        const float scl = 0.1f;

        shader::InstanceCoreData* instanceCoreData = reinterpret_cast<shader::InstanceCoreData*>(insBuff -> contents());

        for (size_t i = 0; i < instances; i++) {

            float divInstances = i / (float) instances;
            float xOff = (divInstances * 2.0f - 1.0f) + (1.f / instances);
            float yOff = sin((divInstances + _angle) * 2.0f * M_PI);

            instanceCoreData[i].instanceTransform = (simd::float4x4) {
                (simd::float4) {scl * sinf(_angle), scl * cosf(_angle), 0.f, 0.f},
                (simd::float4) {scl * cosf(_angle), scl * -sinf(_angle), 0.f, 0.f},
                (simd::float4) {0.f, 0.f, scl, 0.f},
                (simd::float4) {xOff, yOff, 0.f, 1.f}
            };

            float r = divInstances;
            float g = 1.0f - r;
            float b = sinf(M_PI * 2.0f * divInstances);
            
            instanceCoreData[i].instanceColor = (simd::float4) {r, g, b, 1.0f};
        }
        
        insBuff -> didModifyRange(NS::Range::Make(0, insBuff -> length()));

        MTL::RenderPassDescriptor* passDesc = view -> currentRenderPassDescriptor();
        MTL::RenderCommandEncoder* cmdEncoder = cmdBuff -> renderCommandEncoder(passDesc);

        cmdEncoder -> setRenderPipelineState(_pipeState);
        cmdEncoder -> setVertexBuffer(_vertexDataBuff, 0, 0);
        cmdEncoder -> setVertexBuffer(insBuff, 0, 1);
        cmdEncoder -> drawIndexedPrimitives(
            MTL::PrimitiveType::PrimitiveTypeTriangle,
            6,
            MTL::IndexType::IndexTypeUInt16,
            _indexBuff,
            0,
            instances
        );
        cmdEncoder -> endEncoding();

        cmdBuff -> presentDrawable(view -> currentDrawable());
        cmdBuff -> commit();

        pool -> release();
    }

#pragma endregion Render }
