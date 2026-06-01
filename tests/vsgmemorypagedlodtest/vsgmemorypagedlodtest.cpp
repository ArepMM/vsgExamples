#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <iostream>

//#include "DatabasePagerAutoscale.h"

struct MemoryStatistics : public vsg::Inherit<vsg::Visitor, MemoryStatistics>
{
    MemoryStatistics(vsg::ref_ptr<vsg::Device> in_device)
        : _device(in_device) {}

    uint64_t frame_count = 0;
    uint64_t available_CPU_memory = 0;
    uint64_t reserved_CPU_memory = 0;
    uint64_t total_CPU_memory = 0;
    uint64_t available_GPU_memory = 0;
    uint64_t reserved_GPU_memory = 0;
    uint64_t total_GPU_memory = 0;
    uint64_t available_GPU_buffer = 0;
    uint64_t reserved_GPU_buffer = 0;
    uint64_t total_GPU_buffer = 0;

    void apply(vsg::FrameEvent& fe) override
    {
        frame_count = fe.frameStamp->frameCount;

        auto& alloc = vsg::Allocator::instance();
        available_CPU_memory = alloc->totalAvailableSize();
        reserved_CPU_memory = alloc->totalReservedSize();

        auto mem_pools = _device->deviceMemoryBufferPools.ref_ptr();
        available_GPU_memory = mem_pools->computeMemoryTotalAvailable();
        reserved_GPU_memory = mem_pools->computeMemoryTotalReserved();
        available_GPU_buffer = mem_pools->computeBufferTotalAvailable();
        reserved_GPU_buffer = mem_pools->computeBufferTotalReserved();

        total_CPU_memory = available_CPU_memory + reserved_CPU_memory;
        total_GPU_memory = available_GPU_memory + reserved_GPU_memory;
        total_GPU_buffer = available_GPU_buffer + reserved_GPU_buffer;
        if (total_CPU_memory && total_GPU_memory && total_GPU_buffer)
        {
            int available_CPU_memory_percent = 100 * available_CPU_memory / total_CPU_memory;
            int reserved_CPU_memory_percent = 100 * reserved_CPU_memory / total_CPU_memory;
            int available_GPU_memory_percent = 100 * available_GPU_memory / total_GPU_memory;
            int reserved_GPU_memory_percent = 100 * reserved_GPU_memory / total_GPU_memory;
            int available_GPU_buffer_percent = 100 * available_GPU_buffer / total_GPU_buffer;
            int reserved_GPU_buffer_percent = 100 * reserved_GPU_buffer / total_GPU_buffer;

            vsg::info("Frame=", frame_count,
                "  |  RAM:", available_CPU_memory, " (", available_CPU_memory_percent, "%) + ", reserved_CPU_memory, " (", reserved_CPU_memory_percent, "%) = ", total_CPU_memory,
                "  |  VRAM:", available_GPU_memory, " (", available_GPU_memory_percent, "%) + ", reserved_GPU_memory, " (", reserved_GPU_memory_percent, "%) = ", total_GPU_memory,
                "  |  VBuffer:", available_GPU_buffer, " (", available_GPU_buffer_percent, "%) + ", reserved_GPU_buffer, " (", reserved_GPU_buffer_percent, "%) = ", total_GPU_buffer);
        }
    }

private:
    vsg::ref_ptr<vsg::Device> _device;
};

int main(int argc, char** argv)
{
    try
    {
        // set up defaults and read command line arguments to override them
        vsg::CommandLine arguments(&argc, argv);

        // create windowTraits using the any command line arugments to configure settings
        auto windowTraits = vsg::WindowTraits::create(arguments);

        // set up vsg::Options to pass in filepaths, ReaderWriters and other IO related options to use when reading and writing files.
        auto options = vsg::Options::create();
        options->fileCache = vsg::getEnv("VSG_FILE_CACHE");
        options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

#ifdef vsgXchange_all
        // add vsgXchange's support for reading and writing 3rd party file formats
        options->add(vsgXchange::all::create());
#endif

        auto maxPagedLOD = arguments.value(1500, "--maxPagedLOD");
        auto useSharedObjects = arguments.value(false, "--sharedObjects");

        options->readOptions(arguments);

        if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        if (argc <= 1)
        {
            std::cout << "Please specify 3d-models on the command line." << std::endl;
            return 1;
        }

        auto root = vsg::Group::create();

        auto scene = vsg::MatrixTransform::create();
        root->addChild(scene);

        int numModels = argc - 1;
        double angle = 2.0 * vsg::numbers<double>::PI() / static_cast<double>(numModels);
        double shift = 2.0 + numModels;
        vsg::dvec3 origin(0.0, 0.0, 0.0);
        vsg::dvec3 axis_Z(0.0, 0.0, 1.0);

        // read any 3d-models files
        for (int i = 1; i < argc; ++i)
        {
            vsg::Path filename = arguments[i];

            if (vsg::fileExists(filename))
            {
                int index = (i - 1);
                vsg::dvec3 position = origin + vsg::dvec3(0.0, shift, 0.0);
                auto transform = vsg::MatrixTransform::create(  vsg::rotate(index * angle, axis_Z)
                                                              * vsg::translate(position));

                auto pagedLOD = vsg::PagedLOD::create();
                pagedLOD->filename = filename;
                pagedLOD->options = options;
                pagedLOD->bound = vsg::dsphere(origin, 1.0);
                transform->addChild(pagedLOD);
                scene->addChild(transform);

                std::cout << "Sucessfully added pagedLOD for file " << filename << std::endl;
            }
            else
            {
                std::cout << "Unable to find file " << filename << std::endl;
            }
        }

        if (scene->children.empty())
        {
            std::cout << "No 3d models added" << std::endl;
            return 1;
        }

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();
        auto window = vsg::Window::create(windowTraits);
        if (!window)
        {
            std::cout << "Could not create window." << std::endl;
            return 1;
        }

        viewer->addWindow(window);

        // compute position the camera
        double eye_height = 1.0;
        vsg::dvec3 eye = origin + vsg::dvec3(0.0, 0.0, eye_height);
        vsg::dvec3 centre = origin + vsg::dvec3(0.0, shift, 0.0);
        vsg::dvec3 up = vsg::dvec3(0.0, 0.0, 1.0);

        // set up the camera
        auto lookAt = vsg::LookAt::create(eye, centre, up);
        auto perspective = vsg::Perspective::create(30.0,
                                                    static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height),
                                                    0.01,
                                                    1000.0);
        auto viewportState = vsg::ViewportState::create(window->extent2D());
        auto camera = vsg::Camera::create(perspective, lookAt, viewportState);

        // add close handler to respond to the close window button and pressing escape
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));

        viewer->addEventHandler(vsg::Trackball::create(camera));
        auto view = vsg::View::create(camera);
        view->addChild(vsg::createHeadlight());
        view->addChild(scene);

        auto renderGraph = vsg::RenderGraph::create(window, view);
        auto commandGraph = vsg::CommandGraph::create(window, renderGraph);
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
/*
        // set custom databasepager which show each model with the same size for better demo scene
        if (!defaultDatabasePager)
        {
            for (auto& task : viewer->recordAndSubmitTasks)
            {
                task->databasePager = DatabasePagerAutoscale::create();
                std::cout << "Applied custom databasePager with autoscale models to the same size" << std::endl;
            }
        }
*/
        viewer->compile();

        // add memory statistics
        viewer->addEventHandler(MemoryStatistics::create(window->getDevice()));

        // set targetMaxNumPagedLODWithHighResSubgraphs after Viewer::compile() as it will assign any DatabasePager if required.
        if (maxPagedLOD >= 0)
        {
            for (auto& task : viewer->recordAndSubmitTasks)
            {
                if (task->databasePager) task->databasePager->targetMaxNumPagedLODWithHighResSubgraphs = maxPagedLOD;
                std::cout << "Applied databasePager->targetMaxNumPagedLODWithHighResSubgraphs = " << maxPagedLOD << std::endl;
            }
        }

        if (useSharedObjects)
        {
            options->sharedObjects = vsg::SharedObjects::create();
        }

        viewer->start_point() = vsg::clock::now();

        // rendering main loop
        while (viewer->advanceToNextFrame())
        {
            // rotate scene around camera to show and hide the pagedLODs
            double time = viewer->getFrameStamp()->simulationTime;
            scene->matrix = vsg::rotate(time, axis_Z);

            // update memory statistics

            viewer->handleEvents();

            viewer->update();

            viewer->recordAndSubmit();

            viewer->present();
        }
    }
    catch (const vsg::Exception& ve)
    {
        for (int i = 0; i < argc; ++i) std::cerr << argv[i] << " ";
        std::cerr << "\n[Exception] - " << ve.message << " result = " << ve.result << std::endl;
        return 1;
    }

    // clean up done automatically thanks to ref_ptr<>
    return 0;
}
