#include "render.hpp" 

using namespace std;
using namespace glm;
 
tuple<int, int> get_block_xy(int N);

#ifdef DISTANCE_FIELD
struct bp_tex {u8 r; u8 g;};
#else
struct bp_tex {u8 r;};
#endif
//block palette on cpu side is expected to be array of pointers to blocks
void Renderer::update_Block_Palette(Block** blockPalette){
    //block palette is in u8, but for easy copying we convert it to u16 cause block palette is R16_UNIT
    VkDeviceSize bufferSize = (sizeof(bp_tex))*16*BLOCK_PALETTE_SIZE_X* 16*BLOCK_PALETTE_SIZE_Y* 16;
    table3d<bp_tex> blockPaletteLinear = {};
        blockPaletteLinear.allocate(16*BLOCK_PALETTE_SIZE_X, 16*BLOCK_PALETTE_SIZE_Y, 16);
        #ifdef DISTANCE_FIELD
        blockPaletteLinear.set({0,0});
        #else
        blockPaletteLinear.set({0});
        #endif
    // printl(static_block_palette_size)
    for(i32 N=0; N<static_block_palette_size; N++){
        for(i32 x=0; x<BLOCK_SIZE; x++){
        for(i32 y=0; y<BLOCK_SIZE; y++){
        for(i32 z=0; z<BLOCK_SIZE; z++){
            auto [block_x, block_y] = get_block_xy(N);
            // blockPaletteLinear(x+16*block_x, y+16*block_y, z) = blockPalette[N].voxels[x][y][z];
            if(blockPalette[N] == NULL) {
                blockPaletteLinear(x+16*block_x, y+16*block_y, z).r = 0;
            }
            else {
                if(N < static_block_palette_size){
                    blockPaletteLinear(x+16*block_x, y+16*block_y, z).r = (u16) blockPalette[N]->voxels[x][y][z];
                }
                else {
                    blockPaletteLinear(x+16*block_x, y+16*block_y, z).r = 0;
                }
            }
    }}}}
// println
    VkBufferCreateInfo stagingBufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        stagingBufferInfo.size = bufferSize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo stagingAllocInfo = {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        stagingAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    VkBuffer stagingBuffer = {};
    VmaAllocation stagingAllocation = {};
    void* data = NULL;

    VK_CHECK(vmaCreateBuffer(VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, NULL));
    VK_CHECK(vmaMapMemory(VMAllocator, stagingAllocation, &data));

assert(data!=NULL);
assert(bufferSize!=0);
assert(blockPaletteLinear.data()!=NULL);
    memcpy(data, blockPaletteLinear.data(), bufferSize);
    vmaUnmapMemory(VMAllocator, stagingAllocation);

    for(i32 i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        copy_Buffer(stagingBuffer, &origin_block_palette[i], uvec3(16*BLOCK_PALETTE_SIZE_X, 16*BLOCK_PALETTE_SIZE_Y, 16));
    }

    vmaDestroyBuffer(VMAllocator, stagingBuffer, stagingAllocation);
}

//TODO: do smth with frames in flight
void Renderer::update_Material_Palette(Material* materialPalette){
    VkDeviceSize bufferSize = sizeof(Material)*256;

    VkBufferCreateInfo stagingBufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        stagingBufferInfo.size = bufferSize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo stagingAllocInfo = {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        stagingAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    VkBuffer stagingBuffer = {};
    VmaAllocation stagingAllocation = {};
    vmaCreateBuffer(VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, NULL);

    void* data;
    vmaMapMemory(VMAllocator, stagingAllocation, &data);
        memcpy(data, materialPalette, bufferSize);
    vmaUnmapMemory(VMAllocator, stagingAllocation);

    for(i32 i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        copy_Buffer(stagingBuffer, &material_palette[i], uvec3(6,256,1));
    }

    vmaDestroyBuffer(VMAllocator, stagingBuffer, stagingAllocation);
}

char* readFileBuffer(const char* path, size_t* size) {
    FILE* fp = fopen(path, "rb");
    assert(fp != NULL);
    *size = _filelength(_fileno(fp));
    char* buffer = (char*) malloc(*size);
    fread(buffer, *size, 1, fp);
    fclose(fp);

    return buffer;
}

// namespace ogt { //this library for some reason uses ogt_ in cases when it will never intersect with others BUT DOES NOT WHEN IT FOR SURE WILL
#include <ogt_vox.hpp>
#include <ogt_voxel_meshify.hpp>
// } //so we'll have to use ogt::ogt_func :(
// struct scene_file_header = {int }

void Renderer::load_scene(const char* vox_file){
    size_t buffer_size;
    FILE* fp = fopen(vox_file, "rb");
    if(fp == NULL){
        origin_world.set(0);
        return;
    }

    buffer_size = _filelength(_fileno(fp));
    char* buffer = (char*) malloc(buffer_size);
    fread(buffer, buffer_size, 1, fp);
    fclose(fp);


    ivec3* header = (ivec3*)buffer;

    ivec3      stored_world_size = *header;
    BlockID_t* stored_world = (BlockID_t*)(buffer+sizeof(ivec3));

    ivec3 size2read = glm::clamp(stored_world_size, ivec3(0), world_size);
    // buffer
    for(int xx=0; xx< size2read.x; xx++){
    for(int yy=0; yy< size2read.y; yy++){
    for(int zz=0; zz< size2read.z; zz++){
        BlockID_t loaded_block = stored_world[xx + stored_world_size.x*yy + (stored_world_size.x*stored_world_size.y)*zz];        
        origin_world(xx,yy,zz) = glm::clamp(loaded_block, BlockID_t(0), BlockID_t(static_block_palette_size));
    }}}
    free(buffer);
}
void Renderer::save_scene(const char* vox_file){
    FILE* fp = fopen(vox_file, "wb");
    assert(fp != NULL);
    fwrite(&world_size, sizeof(ivec3), 1, fp);
    fwrite(origin_world.data(), sizeof(BlockID_t), world_size.x*world_size.y*world_size.z, fp);
    fclose(fp);
}
//TODO: alloca
void Renderer::load_mesh(Mesh* mesh, const char* vox_file, bool _make_vertices, bool extrude_palette){
    size_t buffer_size;
    char* buffer = readFileBuffer(vox_file, &buffer_size);
    const ogt::vox_scene* scene = ogt::vox_read_scene((u8*)buffer, buffer_size);
    free(buffer);

    assert(scene->num_models == 1);

    load_mesh(mesh, (Voxel*)scene->models[0]->voxel_data, scene->models[0]->size_x, scene->models[0]->size_y, scene->models[0]->size_z, _make_vertices);
    
    if(extrude_palette and not has_palette){
        has_palette = true;
        for(int i=0; i<MATERIAL_PALETTE_SIZE; i++){   
            mat_palette[i].color = vec4(
                scene->palette.color[i].r / 256.0,
                scene->palette.color[i].g / 256.0,
                scene->palette.color[i].b / 256.0,
                scene->palette.color[i].a / 256.0
            );
            // mat_palette[i].emmit = scene->materials.matl[i].;
            mat_palette[i].emmit = 0;
            float rough;
            switch (scene->materials.matl[i].type) {
                case ogt::matl_type_diffuse: {
                    rough = 1.0;
                    break;
                }
                case ogt::matl_type_emit: {
                    mat_palette[i].emmit = scene->materials.matl[i].emit * (2.0 + scene->materials.matl[i].flux*4.0);
                    rough = 0.5;
                    break;
                }
                case ogt::matl_type_metal: {
                    rough = (scene->materials.matl[i].rough + (1.0-scene->materials.matl[i].metal))/2.0;
                    break;
                }
                default: {
                    rough = 0;
                    break;
                } 
            }   
            mat_palette[i].rough = rough;
        }
    }
    
/*
roughness
IOR
Specular
Metallic

Emmission
Power - radiant flux
Ldr
*/
    ogt::vox_destroy_scene(scene);
}

void Renderer::load_block(Block** block, const char* vox_file){
    size_t buffer_size;
    char* buffer = readFileBuffer(vox_file, &buffer_size);
    const ogt::vox_scene* scene = ogt::vox_read_scene((u8*)buffer, buffer_size);
    free(buffer);
    assert(scene->num_models == 1);

    if((*block) == NULL) {
        *block = new Block;
    }

    // load_mesh(&(*block)->mesh, (Voxel*)scene->models[0]->voxel_data, scene->models[0]->size_x, scene->models[0]->size_y, scene->models[0]->size_z, true);
    (*block)->mesh.size = ivec3(scene->models[0]->size_x, scene->models[0]->size_y, scene->models[0]->size_z);
    (*block)->mesh.shift = vec3(0);
    (*block)->mesh.rot = quat_identity<float, defaultp>();
    make_vertices(&((*block)->mesh), (Voxel*)scene->models[0]->voxel_data, scene->models[0]->size_x, scene->models[0]->size_y, scene->models[0]->size_z);
    // printl((*block)->mesh.triangles.Pzz.icount);
    
    for (int x=0; x < scene->models[0]->size_x; x++){
    for (int y=0; y < scene->models[0]->size_y; y++){
    for (int z=0; z < scene->models[0]->size_z; z++){
        (*block)->voxels[x][y][z] = (u16) scene->models[0]->voxel_data[x + y*scene->models[0]->size_x + z*scene->models[0]->size_x*scene->models[0]->size_y];
    }}}
    
    ogt::vox_destroy_scene(scene);
}

#define free_helper(dir) \
if(not (*block)->mesh.triangles.dir. indexes.empty()) vmaDestroyBuffer(VMAllocator, (*block)->mesh.triangles.dir. indexes[i].buffer, (*block)->mesh.triangles.dir. indexes[i].alloc);
void Renderer::free_block(Block** block){
    for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        // if(not (*block)->mesh.triangles.Pzz.vertexes.empty()) vmaDestroyBuffer(VMAllocator, (*block)->mesh.vertexes[i].buffer, (*block)->mesh.vertexes[i].alloc);
        // if(not (*block)->mesh. indexes.empty()) vmaDestroyBuffer(VMAllocator, (*block)->mesh. indexes[i].buffer, (*block)->mesh. indexes[i].alloc);
        free_helper(Pzz);
        free_helper(Nzz);
        free_helper(zPz);
        free_helper(zNz);
        free_helper(zzP);
        free_helper(zzN);
        if(not (*block)->mesh.triangles.vertexes.empty()) vmaDestroyBuffer(VMAllocator, (*block)->mesh.triangles.vertexes[i].buffer, (*block)->mesh.triangles.vertexes[i].alloc);\
    }

    if(not((*block)==NULL)) delete (*block);
}

void Renderer::load_mesh(Mesh* mesh, Voxel* Voxels, int x_size, int y_size, int z_size, bool _make_vertices){
    mesh->size = ivec3(x_size, y_size, z_size);
    if(_make_vertices){
        make_vertices(mesh, Voxels, x_size, y_size, z_size);
    }

    // table3d<Voxel> voxels_extended = {};
    // voxels_extended.allocate(x_size, y_size, z_size);
    // for (int x=0; x < x_size; x++){
    // for (int y=0; y < y_size; y++){
    // for (int z=0; z < z_size; z++){
    //     voxels_extended(x,y,z) = (Voxel) Voxels[x + y*x_size + z*x_size*y_size];
    // }}}

    mesh->voxels = create_RayTrace_VoxelImages(Voxels, mesh->size);
    mesh->shift = vec3(0);
    mesh->rot = quat_identity<float, defaultp>();
    
    // voxels_extended.deallocate();
}
//frees only gpu side stuff, not mesh ptr
#undef  free_helper
#define free_helper(dir) \
if(not mesh->triangles.dir. indexes.empty()) vmaDestroyBuffer(VMAllocator, mesh->triangles.dir. indexes[i].buffer, mesh->triangles.dir. indexes[i].alloc);
void Renderer::free_mesh(Mesh* mesh){
    for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        // if(not mesh->vertexes.empty()) vmaDestroyBuffer(VMAllocator, mesh->vertexes[i].buffer, mesh->vertexes[i].alloc);
        free_helper(Pzz);
        free_helper(Nzz);
        free_helper(zPz);
        free_helper(zNz);
        free_helper(zzP);
        free_helper(zzN);
        if(not mesh->triangles.vertexes.empty()) vmaDestroyBuffer(VMAllocator, mesh->triangles.vertexes[i].buffer, mesh->triangles.vertexes[i].alloc);\
        // if(not mesh->indexes.empty()) vmaDestroyBuffer(VMAllocator, mesh->indexes[i].buffer, mesh->indexes[i].alloc);
        // if(not mesh->voxels.empty()) vmaDestroyImage(VMAllocator, mesh->voxels[i].image, mesh->voxels[i].alloc);
        // if(not mesh->voxels.empty()) vkDestroyImageView(device, mesh->voxels[i].view, NULL);
        // free_helper()
    }
#undef free_helper
}

// #define 
// static unsigned char pack_normal(vec<3, unsigned char, defaultp> normal){

// }
void Renderer::make_vertices(Mesh* mesh, Voxel* Voxels, int x_size, int y_size, int z_size){
    ogt::ogt_voxel_meshify_context ctx = {};
    // ogt::ogt_mesh* ogt_mesh = ogt::ogt_mesh_from_paletted_voxels_polygon(&ctx, (const u8*)Voxels, x_size, y_size, z_size, NULL);
    // ogt::ogt_mesh_remove_duplicate_vertices(&ctx, ogt_mesh);
    ogt::ogt_int_mesh* ogt_mesh =  ogt::my_int_mesh_from_paletted_voxels(&ctx, (const u8*)Voxels, x_size, y_size, z_size);
    ogt::my_int_mesh_optimize(&ctx, ogt_mesh);

// println
    vector<VoxelVertex> verts(ogt_mesh->vertex_count);
    for(u32 i=0; i<ogt_mesh->vertex_count; i++){
        verts[i].pos  = uvec3(ogt_mesh->vertices[i].pos);
        verts[i].norm = ivec3(ogt_mesh->vertices[i].normal);
        verts[i].matID = (MatID_t)ogt_mesh->vertices[i].palette_index;

        assert(verts[i].norm != (vec<3, signed char, defaultp>)(0));
        assert(length(vec3(verts[i].norm)) == 1.0f);
    }

    vector<u32> verts_Pzz = {};
    vector<u32> verts_Nzz = {};
    vector<u32> verts_zPz = {};
    vector<u32> verts_zNz = {};
    vector<u32> verts_zzP = {};
    vector<u32> verts_zzN = {};

    for(u32 i=0; i<ogt_mesh->index_count; i++){
        u32 index = ogt_mesh->indices[i];
        u32 provoking_index = ogt_mesh->indices[(i / 3) * 3];
        vec<3, signed char, defaultp> norm = verts[provoking_index].norm;

        //where is my clang-tidy disable specific warning #pragma?..
        if     (norm == vec<3, signed char, defaultp>(+1,0,0)) verts_Pzz.push_back(index);
        else if(norm == vec<3, signed char, defaultp>(-1,0,0)) verts_Nzz.push_back(index);
        else if(norm == vec<3, signed char, defaultp>(0,+1,0)) verts_zPz.push_back(index);
        else if(norm == vec<3, signed char, defaultp>(0,-1,0)) verts_zNz.push_back(index);
        else if(norm == vec<3, signed char, defaultp>(0,0,+1)) verts_zzP.push_back(index);
        else if(norm == vec<3, signed char, defaultp>(0,0,-1)) verts_zzN.push_back(index);
        else {
            printl((int)norm.x);
            printl((int)norm.y);
            printl((int)norm.z);
            crash(unrecognized normal);
        }
    }
// println
    mesh->triangles.vertexes = create_elemBuffers<VoxelVertex>(verts.data(), ogt_mesh->vertex_count, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
// println
    // printl(verts_Pzz.data());
    // printl(verts_Pzz.size());

    mesh->triangles.Pzz.indexes = create_elemBuffers<u32>(verts_Pzz.data(), verts_Pzz.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    mesh->triangles.Nzz.indexes = create_elemBuffers<u32>(verts_Nzz.data(), verts_Nzz.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    mesh->triangles.zPz.indexes = create_elemBuffers<u32>(verts_zPz.data(), verts_zPz.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    mesh->triangles.zNz.indexes = create_elemBuffers<u32>(verts_zNz.data(), verts_zNz.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    mesh->triangles.zzP.indexes = create_elemBuffers<u32>(verts_zzP.data(), verts_zzP.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    mesh->triangles.zzN.indexes = create_elemBuffers<u32>(verts_zzN.data(), verts_zzN.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
// println
    assert(verts_Pzz.size()!=0);
    assert(verts_Nzz.size()!=0);
    assert(verts_zPz.size()!=0);
    assert(verts_zNz.size()!=0);
    assert(verts_zzP.size()!=0);
    assert(verts_zzN.size()!=0);
    mesh->triangles.Pzz.icount = verts_Pzz.size();
    mesh->triangles.Nzz.icount = verts_Nzz.size();
    mesh->triangles.zPz.icount = verts_zPz.size();
    mesh->triangles.zNz.icount = verts_zNz.size();
    mesh->triangles.zzP.icount = verts_zzP.size();
    mesh->triangles.zzN.icount = verts_zzN.size();
    // mesh->icount = ogt_mesh->index_count;
    mesh->shift = vec3(0);
    mesh->rot = quat_identity<float, defaultp>();
    // ogt::ogt_mesh_destroy(&ctx, int_mesh);
    ogt::ogt_mesh_destroy(&ctx, (ogt::ogt_mesh*)ogt_mesh);
}
