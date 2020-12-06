//#ifndef MWVR_VRVIEW_H
//#define MWVR_VRVIEW_H
//
//#include <cassert>
//#include "openxrmanager.hpp"
//#include "openxrswapchain.hpp"
//
//struct XrSwapchainSubImage;
//
//namespace MWVR
//{
//    class VRViewer;
//
//    /// \brief Manipulates a slave camera by replacing its framebuffer with one destined for openxr
//    class VRView : public osg::Referenced
//    {
//    public:
//
//        class InitialDrawCallback : public osg::Camera::DrawCallback
//        {
//        public:
//            virtual void operator()(osg::RenderInfo& renderInfo) const;
//        };
//
//        class UpdateSlaveCallback : public osg::View::Slave::UpdateSlaveCallback
//        {
//        public:
//            UpdateSlaveCallback(osg::ref_ptr<VRView> view) : mView(view) {}
//            void updateSlave(osg::View& view, osg::View::Slave& slave) override;
//
//        private:
//            osg::ref_ptr<VRView> mView;
//        };
//
//    public:
//        VRView(std::string name, SwapchainConfig config, osg::ref_ptr<osg::State> state);
//        virtual ~VRView();
//
//    public:
//        //! Prepare for render (set FBO)
//        virtual void prerenderCallback(osg::RenderInfo& renderInfo);
//
//        //! Finalize render
//        virtual void postrenderCallback(osg::RenderInfo& renderInfo);
//
//        //! Create camera for this view
//        osg::Camera* createCamera(int order, const osg::Vec4& clearColor, osg::GraphicsContext* gc);
//
//        //! Get the view surface
//        OpenXRSwapchain& swapchain(void) { return *mSwapchain; }
//
//        //! Present to the openxr swapchain
//        void swapBuffers(osg::GraphicsContext* gc);
//
//        void updateSlave(osg::View& view, osg::View::Slave& slave);
//    public:
//        SwapchainConfig mSwapchainConfig;
//        std::unique_ptr<OpenXRSwapchain> mSwapchain;
//        std::string mName{};
//        osg::Node::NodeMask mCullMask;
//        bool mRendering{ false };
//    };
//
//    class MyVRView
//    {
//    public:
//        class InitialDrawCallback : public osg::Camera::DrawCallback
//        {
//        public:
//            virtual void operator()(osg::RenderInfo& renderInfo) const;
//        };
//
//        class UpdateSlaveCallback : public osg::View::Slave::UpdateSlaveCallback
//        {
//        public:
//            UpdateSlaveCallback(MyVRView* view) : mView(view) {}
//            void updateSlave(osg::View& view, osg::View::Slave& slave) override;
//
//        private:
//            MyVRView* mView;
//        };
//
//    public:
//        MyVRView(std::string name, SwapchainConfig config, osg::ref_ptr<osg::State> state);
//        virtual ~MyVRView();
//
//    public:
//        //! Prepare for render (set FBO)
//        virtual void prerenderCallback(osg::RenderInfo& renderInfo);
//
//        //! Finalize render
//        virtual void postrenderCallback(osg::RenderInfo& renderInfo);
//
//        //! Create camera for this view
//        osg::Camera* createCamera(int order, const osg::Vec4& clearColor, osg::GraphicsContext* gc);
//
//        //! Get the view surface
//        OpenXRSwapchain& swapchain(void) { return *mSwapchain; }
//
//        //! Present to the openxr swapchain
//        void swapBuffers(osg::GraphicsContext* gc);
//
//        void updateSlave(osg::View& view, osg::View::Slave& slave);
//
//        SwapchainConfig mSwapchainConfig;
//        std::unique_ptr<OpenXRSwapchain> mSwapchain;
//        std::string mName{};
//        osg::Node::NodeMask mCullMask;
//        bool mRendering{ false };
//    };
//}
//
//#endif
