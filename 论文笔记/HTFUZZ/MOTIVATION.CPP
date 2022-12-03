typedef struct{
    const struct box_registry_entry *registry;
    ...
} GF_Box;
static struct box_registry_entry {
    GF_Box * (*new_fn)();
    void (*del_fn)(GF_Box *a);
    GF_Err (*read_fn)(GF_Box *s, GF_BitStream *bs);
    ...
} box_registry;
        
//isomedia/box_funcs.c
void gf_isom_box_del(GF_Box *a) {
    other_boxes = a->other_boxes; { use()} //crash
    if (!a->registry && use_dump_mode)
    a->registry->del_fn(a); // del_fn =free,...
}
GF_Box *gf_isom_box_new_ex(...) {
    a = box_registry[idx].new_fn(); //new_fn =stco_New,...
}
GF_Err gf_isom_box_read(GF_Box *a, GF_BitStream *bs) {
    return a->registry->read_fn(a,bs); //read_fn =stbl_Read,minf_Read...
}
GF_Err gf_isom_box_parse_ex(...){
    if (...)
    newBox = gf_isom_box_new_ex(...); ->stco_New() ->malloc()
    if (...)
    e = gf_isom_box_read(newBox, bs);
}
//isomedia/box_code_base.c
GF_Err stbl_AddBox(GF_Box *s, GF_Box *a) {
    switch (a->type) {
        case GF_ISOM_BOX_TYPE_STCO:
        gf_isom_box_del(s->ChunkOffset); ->free();
        }}
GF_Err stbl_Read(GF_Box *s, GF_BitStream *bs) {
    e = gf_isom_box_array_read(s, bs, stbl_AddBox);
    CALL(gf_isom_box_array_read_ex(..., GF_Err (*add_box)))//do callback
    {
        while (parent->size>=8){
            e = gf_isom_box_parse_ex(...); ->gf_isom_box_new_ex()
            e = add_box(parent, a); //if add_box = stbl_AddBox
}}}
//isomedia/box_funcs.c
GF_Err gf_isom_box_array_read_ex(...,GF_Err(*add_box)(GF_Box*,GF_Box*)) {
    GF_Err e;
    GF_Box *a = NULL;
    while (parent->size>=8){
        e = gf_isom_box_parse_ex(&a, ...); ->gf_isom_box_read()
        if (e && a) gf_isom_box_del(a);
        e = add_box(parent, a);
        ...
}}
int main(){
    ...
    -> gf_isom_box_array_read_ex(...);
}