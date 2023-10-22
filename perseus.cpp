#include <cassert>

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION

#include <AppKit/AppKit.hpp>
#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>

#pragma region Declaration {

    class Render {

        public:

            Render(MTL::Device* device);

            ~Render();

            void draw(MTK::View* view);

        private:

            MTL::Device* _device;
            MTL::CommandQueue* _commandQueue;
            
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
            {512.0, 512.0}
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
        _view -> setClearColor(MTL::ClearColor::Make(0.0, 0.0, 0.0, 0.0));

        _coreViewDelegate = new CoreViewDelegate(_device);

        _view -> setDelegate(_coreViewDelegate);

        _window -> setContentView(_view);
        _window -> setTitle(NS::String::string("Perseus", NS::StringEncoding::UTF8StringEncoding));
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
    }

    Render::~Render() {
        _commandQueue -> release();
        _device -> release();
    }

    void Render::draw(MTK::View* view) {

        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc() -> init();

        MTL::CommandBuffer* cmdBuff = _commandQueue -> commandBuffer();
        MTL::RenderPassDescriptor* passDesc = view -> currentRenderPassDescriptor();
        MTL::RenderCommandEncoder* cmdEncoder = cmdBuff -> renderCommandEncoder(passDesc);

        cmdEncoder -> endEncoding();

        cmdBuff -> presentDrawable(view -> currentDrawable());
        cmdBuff -> commit();

        pool -> release();
    }

#pragma endregion Render }
