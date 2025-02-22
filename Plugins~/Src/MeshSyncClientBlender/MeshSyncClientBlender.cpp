#include "pch.h"
#include "msblenBinder.h"
#include "msblenContext.h"
#include "BlenderPyObjects/BlenderPyContext.h"

#include "MeshSyncClient/ExportTarget.h"

namespace bl = blender;

static Depsgraph* DepsgraphFromPyObject(py::object graphObj) {
    Depsgraph* graph;
    BPy_StructRNA* rna = (BPy_StructRNA*)graphObj.ptr();
    auto typeName = rna->ob_base.ob_type->tp_name;

    if (strcmp(typeName, "Depsgraph") != 0) {
        return nullptr;
    }

    blender::rna_sdata(graphObj, graph);

    return graph;
}

static bool msblenSend(msblenContext& self, MeshSyncClient::ExportTarget target, MeshSyncClient::ObjectScope scope)
{
    if (!self.isServerAvailable()) {
        self.logInfo("MeshSync: Server not available. %s", self.getErrorMessage().c_str());
        return false;
    }

    if (target == MeshSyncClient::ExportTarget::Objects) {
        self.wait();
        self.sendObjects(MeshSyncClient::ObjectScope::All, true);
    }
    else if (target == MeshSyncClient::ExportTarget::Materials) {
        self.wait();
        self.sendMaterials(true);
    }
    else if (target == MeshSyncClient::ExportTarget::Animations) {
        self.wait();
        self.sendAnimations(MeshSyncClient::ObjectScope::All);
    }
    else if (target == MeshSyncClient::ExportTarget::Everything) {
        self.wait();
        self.sendMaterials(true);
        self.wait();
        self.sendObjects(MeshSyncClient::ObjectScope::All, true);
        self.wait();
        self.sendAnimations(MeshSyncClient::ObjectScope::All);
    }
    return true;
}

class ContextProxy
{
public:
    ContextProxy() {}
    ~ContextProxy() {}
    msblenContext* operator->() { return &msblenGetContext(); }
    const msblenContext* operator->() const { return &msblenGetContext(); }
    msblenContext& operator*() { return msblenGetContext(); }
    const msblenContext& operator*() const { return msblenGetContext(); }
};

class CacheProxy
{
public:
    CacheProxy() {}
    ~CacheProxy() {}
    msblenContext* operator->() { return &msblenGetContext(); }
    const msblenContext* operator->() const { return &msblenGetContext(); }
    msblenContext& operator*() { return msblenGetContext(); }
    const msblenContext& operator*() const { return msblenGetContext(); }
};


PYBIND11_MODULE(MeshSyncClientBlender, m)
{
    m.doc() = "Python bindings for MeshSync";

#define BindConst(Name, ...) .def_property_readonly(#Name, [](self_t& self) { return __VA_ARGS__; })
#define BindMethod(Name, ...) .def(#Name, __VA_ARGS__)
#define BindProperty(Name, ...) .def_property(#Name, __VA_ARGS__)
    {
        using self_t = ContextProxy;
        py::class_<ContextProxy>(m, "Context")
            .def(py::init<>())
            BindConst(PLUGIN_VERSION, std::string(MESHSYNC_DCC_PLUGIN_VER))
            BindConst(PROTOCOL_VERSION, std::to_string(msProtocolVersion))

            BindConst(TARGET_OBJECTS, (int)MeshSyncClient::ExportTarget::Objects)
            BindConst(TARGET_MATERIALS, (int)MeshSyncClient::ExportTarget::Materials)
            BindConst(TARGET_ANIMATIONS, (int)MeshSyncClient::ExportTarget::Animations)
            BindConst(TARGET_EVERYTHING, (int)MeshSyncClient::ExportTarget::Everything)

            BindConst(SCOPE_ALL, (int)MeshSyncClient::ObjectScope::All)
            BindConst(SCOPE_UPDATED, (int)MeshSyncClient::ObjectScope::Updated)
            BindConst(SCOPE_SELECTED, (int)MeshSyncClient::ObjectScope::Selected)

            BindConst(FRANGE_CURRENT, (int)MeshSyncClient::FrameRange::Current)
            BindConst(FRANGE_ALL, (int)MeshSyncClient::FrameRange::All)
            BindConst(FRANGE_CUSTOM, (int)MeshSyncClient::FrameRange::Custom)

            BindConst(MFRANGE_NONE, (int)MeshSyncClient::MaterialFrameRange::None)
            BindConst(MFRANGE_ONE, (int)MeshSyncClient::MaterialFrameRange::One)
            BindConst(MFRANGE_ALL, (int)MeshSyncClient::MaterialFrameRange::All)

            BindConst(is_server_available, self->isServerAvailable())
            BindConst(error_message, self->getErrorMessage())

            BindProperty(server_address,
                [](const self_t& self) { return self->getSettings().client_settings.server; },
                [](self_t& self, const std::string& v) { self->getSettings().client_settings.server = v; })
            BindProperty(server_port,
                [](const self_t& self) { return self->getSettings().client_settings.port; },
                [](self_t& self, uint16_t v) { self->getSettings().client_settings.port = v; })
            BindProperty(scale_factor,
                [](const self_t& self) { return self->getSettings().scene_settings.scale_factor; },
                [](self_t& self, float v) { self->getSettings().scene_settings.scale_factor = v; })
            BindProperty(handedness,
                [](const self_t& self) { return (int)self->getSettings().scene_settings.handedness; },
                [](self_t& self, int v) { (int&)self->getSettings().scene_settings.handedness = v; })
            BindProperty(sync_meshes,
                [](const self_t& self) { return (int)self->getSettings().sync_meshes; },
                [](self_t& self, bool v) { self->getSettings().sync_meshes = v; })
            BindProperty(sync_normals,
                [](const self_t& self) { return (int)self->getSettings().sync_normals; },
                [](self_t& self, bool v) { self->getSettings().sync_normals = v; })
            BindProperty(sync_uvs,
                [](const self_t& self) { return self->getSettings().sync_uvs; },
                [](self_t& self, bool v) { self->getSettings().sync_uvs = v; })
            BindProperty(sync_colors,
                [](const self_t& self) { return self->getSettings().sync_colors; },
                [](self_t& self, bool v) { self->getSettings().sync_colors = v; })
            BindProperty(make_double_sided,
                [](const self_t& self) { return self->getSettings().make_double_sided; },
                [](self_t& self, bool v) { self->getSettings().make_double_sided = v; })
            BindProperty(bake_modifiers,
                [](const self_t& self) { return self->getSettings().BakeModifiers; },
                [](self_t& self, bool v) { self->getSettings().BakeModifiers = v; })
            BindProperty(bake_transform,
                [](const self_t& self) { return self->getSettings().BakeTransform; },
                [](self_t& self, bool v) { self->getSettings().BakeTransform = v; })
            BindProperty(curves_as_mesh,
                [](const self_t& self) { return self->getSettings().curves_as_mesh; },
                [](self_t& self, bool v) { self->getSettings().curves_as_mesh = v; })
            BindProperty(sync_bones,
                [](const self_t& self) { return self->getSettings().sync_bones; },
                [](self_t& self, bool v) { self->getSettings().sync_bones = v; })
            BindProperty(sync_blendshapes,
                [](const self_t& self) { return self->getSettings().sync_blendshapes; },
                [](self_t& self, bool v) { self->getSettings().sync_blendshapes = v; })
            BindProperty(sync_textures,
                [](const self_t& self) { return self->getSettings().sync_textures; },
                [](self_t& self, bool v) { self->getSettings().sync_textures = v; })
            BindProperty(sync_cameras,
                [](const self_t& self) { return self->getSettings().sync_cameras; },
                [](self_t& self, bool v) { self->getSettings().sync_cameras = v; })
            BindProperty(sync_lights,
                [](const self_t& self) { return self->getSettings().sync_lights; },
                [](self_t& self, bool v) { self->getSettings().sync_lights = v; })
            BindProperty(frame_step,
                [](const self_t& self) { return self->getSettings().frame_step; },
                [](self_t& self, int v) { self->getSettings().frame_step = v; })
            BindProperty(multithreaded,
                [](const self_t& self) { return self->getSettings().multithreaded; },
                [](self_t& self, int v) { self->getSettings().multithreaded = v; })

            BindMethod(flushPendingList, [](self_t& self) { self->flushPendingList(); })
            BindMethod(Destroy, [](self_t& self) { self->Destroy(); })
            BindMethod(setup, [](self_t& self, py::object ctx) { bl::setup(ctx); })
            BindMethod(clear, [](self_t& self) { self->clear(); })
            BindMethod(exportUpdatedObjects, [](self_t& self) { self->sendObjects(MeshSyncClient::ObjectScope::Updated, false); })
            BindMethod(export, [](self_t& self, int _target) { msblenSend(*self, (MeshSyncClient::ExportTarget)_target, MeshSyncClient::ObjectScope::All); })
            BindMethod(OnDepsgraphUpdatePost,
                [](self_t& self, py::object depsgraph) {
                    auto graph = DepsgraphFromPyObject(depsgraph);
                    self->onDepsgraphUpdatedPost(graph);
                });
    }
    {
        using self_t = CacheProxy;
        py::class_<CacheProxy>(m, "Cache")
            .def(py::init<>())
            BindProperty(object_scope,
                [](const self_t& self) { return (int)self->getCacheSettings().object_scope; },
                [](self_t& self, int v) { self->getCacheSettings().object_scope = (MeshSyncClient::ObjectScope)v; })
            BindProperty(frame_range,
                [](const self_t& self) { return (int)self->getCacheSettings().frame_range; },
                [](self_t& self, int v) { self->getCacheSettings().frame_range = (MeshSyncClient::FrameRange)v; })
            BindProperty(material_frame_range,
                [](const self_t& self) { return (int)self->getCacheSettings().material_frame_range; },
                [](self_t& self, int v) { self->getCacheSettings().material_frame_range = (MeshSyncClient::MaterialFrameRange)v; })
            BindProperty(frame_begin,
                [](const self_t& self) { return self->getCacheSettings().frame_begin; },
                [](self_t& self, int v) { self->getCacheSettings().frame_begin = v; })
            BindProperty(frame_end,
                [](const self_t& self) { return self->getCacheSettings().frame_end; },
                [](self_t& self, int v) { self->getCacheSettings().frame_end = v; })
            BindProperty(zstd_compression_level,
                [](const self_t& self) { return self->getCacheSettings().zstd_compression_level; },
                [](self_t& self, int v) { self->getCacheSettings().zstd_compression_level = v; })
            BindProperty(frame_step,
                [](const self_t& self) { return self->getCacheSettings().frame_step; },
                [](self_t& self, int v) { self->getCacheSettings().frame_step = v; })
            BindProperty(make_double_sided,
                [](const self_t& self) { return self->getCacheSettings().make_double_sided; },
                [](self_t& self, bool v) { self->getCacheSettings().make_double_sided = v; })
            BindProperty(bake_modifiers,
                [](const self_t& self) { return self->getCacheSettings().bake_modifiers; },
                [](self_t& self, bool v) { self->getCacheSettings().bake_modifiers = v; })
            BindProperty(bake_transform,
                [](const self_t& self) { return self->getCacheSettings().bake_transform; },
                [](self_t& self, bool v) { self->getCacheSettings().bake_transform = v; })
            BindProperty(curves_as_mesh,
                [](const self_t& self) { return self->getCacheSettings().curves_as_mesh; },
                [](self_t& self, bool v) { self->getCacheSettings().curves_as_mesh = v; })
            BindProperty(flatten_hierarchy,
                [](const self_t& self) { return self->getCacheSettings().flatten_hierarchy; },
                [](self_t& self, bool v) { self->getCacheSettings().flatten_hierarchy = v; })
            BindProperty(merge_meshes,
                [](const self_t& self) { return self->getCacheSettings().merge_meshes; },
                [](self_t& self, bool v) { self->getCacheSettings().merge_meshes = v; })
            BindProperty(strip_normals,
                [](const self_t& self) { return self->getCacheSettings().strip_normals; },
                [](self_t& self, bool v) { self->getCacheSettings().strip_normals = v; })
            BindProperty(strip_tangents,
                [](const self_t& self) { return self->getCacheSettings().strip_tangents; },
                [](self_t& self, bool v) { self->getCacheSettings().strip_tangents = v; })


            BindMethod(export, [](self_t& self, std::string path) {
                BlenderCacheSettings settings = msblenContext::getInstance().getCacheSettings(); // copy
                self->ExportCache(path, settings);
            })
            ;
    }
#undef BindConst
#undef BindMethod
#undef BindProperty
}
