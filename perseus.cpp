#include <cassert>

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION

#include <AppKit/AppKit.hpp>
#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>

#include <simd/simd.h>

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
            MTL::Buffer* _argBuff;
            MTL::Buffer* _vertexPositionsBuff;
            MTL::Buffer* _vertexColorsBuff;
            
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

    Render::Render(MTL::Device* device) : _device(device -> retain()) {
        _commandQueue = _device -> newCommandQueue();

        buildShaders();
        buildBuffers();
    }

    Render::~Render() {
        _shaderLibrary -> release();
        _argBuff -> release();
        _vertexPositionsBuff -> release();
        _vertexColorsBuff -> release();
        _pipeState -> release();
        _commandQueue -> release();
        _device -> release();
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

                device float3* positions [[id(0)]];
                device float3* colors [[id(1)]];

            };

            v2f vertex vertexCore(uint vertexId [[vertex_id]],
                device const VertexCoreData* vertexCoreData [[buffer(0)]]) {

                    v2f out;

                    out.position = float4(vertexCoreData -> positions[vertexId], 1.0);
                    out.color = half3(vertexCoreData -> colors[vertexId]);

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

        const size_t numVerticles = 3;

        simd::float3 positions[numVerticles] = {
            {-0.8f, 0.8f, 0.0f},
            {0.0f, -0.8f, 0.0f},
            {+0.8f, 0.8f, 0.0f}
        };

        simd::float3 colors[numVerticles] = {
            {1.0f, 0.3f, 0.2f},
            {0.8f, 1.0, 0.0f},
            {0.8f, 0.0f, 1.0}
        };

        const size_t positionsSize = numVerticles * sizeof(simd::float3);
        const size_t colorsSize = numVerticles * sizeof(simd::float3);

        MTL::Buffer* vPosBuff = _device -> newBuffer(positionsSize, MTL::ResourceStorageModeManaged);
        MTL::Buffer* vColBuff = _device -> newBuffer(colorsSize, MTL::ResourceStorageModeManaged);

        _vertexPositionsBuff = vPosBuff;
        _vertexColorsBuff = vColBuff;

        memcpy(_vertexPositionsBuff -> contents(), positions, positionsSize);
        memcpy(_vertexColorsBuff -> contents(), colors, colorsSize);

        _vertexPositionsBuff -> didModifyRange(NS::Range::Make(0, _vertexPositionsBuff -> length()));
        _vertexColorsBuff -> didModifyRange(NS::Range::Make(0, _vertexColorsBuff -> length()));

        using NS::UTF8StringEncoding;

        assert(_shaderLibrary);

        MTL::Function* vFn = _shaderLibrary -> newFunction(NS::String::string("vertexCore", UTF8StringEncoding));
        MTL::ArgumentEncoder* argEncoder = vFn -> newArgumentEncoder(0);

        MTL::Buffer* argBuff = _device -> newBuffer(argEncoder -> encodedLength(), MTL::ResourceStorageModeManaged);

        _argBuff = argBuff;

        argEncoder -> setArgumentBuffer(_argBuff, 0);
        argEncoder -> setBuffer(_vertexPositionsBuff, 0, 0);
        argEncoder -> setBuffer(_vertexColorsBuff, 0, 1);

        argBuff -> didModifyRange(NS::Range::Make(0, _argBuff -> length()));

        vFn -> release();
        argEncoder -> release();
    }

    void Render::draw(MTK::View* view) {

        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc() -> init();

        MTL::CommandBuffer* cmdBuff = _commandQueue -> commandBuffer();
        MTL::RenderPassDescriptor* passDesc = view -> currentRenderPassDescriptor();
        MTL::RenderCommandEncoder* cmdEncoder = cmdBuff -> renderCommandEncoder(passDesc);

        cmdEncoder -> setRenderPipelineState(_pipeState);
        cmdEncoder -> setVertexBuffer(_argBuff, 0, 0);
        cmdEncoder -> useResource(_vertexPositionsBuff, MTL::ResourceUsageRead);
        cmdEncoder -> useResource(_vertexColorsBuff, MTL::ResourceUsageRead);
        cmdEncoder -> drawPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
        cmdEncoder -> endEncoding();

        cmdBuff -> presentDrawable(view -> currentDrawable());
        cmdBuff -> commit();

        pool -> release();
    }

#pragma endregion Render }
