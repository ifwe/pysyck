
#include <stdio.h>
#include <string.h>

#include <syck.h>

#define VALUE1 "this scalar contains trailing spaces  "
#define VALUE2 "this scalar contains a trailing colon:"

void output_handler(SyckEmitter *e, char *str, long len)
{
    fwrite(str, 1, len, stdout);
}

void emitter_handler(SyckEmitter *e, st_data_t id)
{
    switch (id) {
        case 1:
            syck_emit_seq(e, NULL, seq_none);
            syck_emit_item(e, 2);
            syck_emit_item(e, 3);
            syck_emit_end(e);
            break;
        case 2:
            syck_emit_scalar(e, "tag:yaml.org,2002:str", scalar_none, 0, 0, 0, VALUE1, strlen(VALUE1));
            break;
        case 3:
            syck_emit_scalar(e, "tag:yaml.org,2002:str", scalar_none, 0, 0, 0, VALUE2, strlen(VALUE2));
            break;
    }
        
}

int main(int argc, char *argv[])
{
    SyckEmitter *e;

    e = syck_new_emitter();
    syck_emitter_handler(e, emitter_handler);
    syck_output_handler(e, output_handler);
    syck_emitter_mark_node(e, 1);
    syck_emitter_mark_node(e, 2);
    syck_emitter_mark_node(e, 3);
    syck_emit(e, 1);
    syck_emitter_flush(e, 0);
    syck_free_emitter(e);
}

