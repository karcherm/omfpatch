const int dbg = 0;

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SEGS 20
#define MAX_CHKS 5

typedef struct {
    char name[17];
    long fileofs;
    unsigned base;
    unsigned limit;
    int nameidx;
    int segidx;
} seg_t;

typedef enum { CHKS_SUM8 } checksumtype;
typedef struct {
    long firstofs;
    long lastofs;
    long sumofs;
    checksumtype type;
} chks_t;

// the code USES the property that globals are zero-initialized,
// and relies on nameidx and segidx being zero (an invalid obj index)
seg_t segtable[MAX_SEGS];
chks_t checksums[MAX_CHKS];

unsigned segs;
unsigned chks;
unsigned nameidx = 0;
unsigned segidx = 0;

void DIE(const char* msg, ...)
{
    va_list va;
    va_start(va, msg);
    vfprintf(stderr, msg, va);
    exit(1);
}

void load_segs(const char* fname)
{
    char linebuf[80];
    FILE* f = fopen(fname, "r");
    if (!f)
        DIE("Can't open segment map %s\n", fname);
    while (fgets(linebuf, sizeof linebuf, f))
    {
        char dummy;
        seg_t *s = &segtable[segs];
        unsigned linelen = strlen(linebuf);
        if (!linelen ||
            (linelen == 1 && linebuf[0] == '\n'))
            continue;
        if (linebuf[0] == '#')
            continue;
        if (linebuf[0] == '!')
        {
            if (strncmp(linebuf + 1, "CHKSUM", 6) == 0)
            {
                char typebuf[9];
                chks_t *c = &checksums[chks];
                if (sscanf(linebuf + 7, "%8s %li %li %li %c",
                           typebuf, &c->firstofs, &c->lastofs,
                                    &c->sumofs, &dummy) != 4)
                    DIE("malformed checksum specification\n%s\n", linebuf);
                if (c->firstofs > c->lastofs)
                    DIE("checksum area start %lx after end %lx\n",
                        c->firstofs, c->lastofs);
                if(strcmp(typebuf, "SUM8") == 0)
                    c->type = CHKS_SUM8;
                else
                    DIE("unknown checksum algorithm %s\n", typebuf);
                chks++;
            }
            else
                DIE("unknown directive\n%s\n", linebuf);
            continue;
        }
        if (segs == MAX_SEGS)
            DIE("Too many segments\n");
        if (linebuf[linelen-1] != '\n' && linelen == sizeof(linebuf) - 1)
            DIE("Overly long map file line starting with\n%s\n", linebuf);
        if (sscanf(linebuf, "%16s %li %i %i %c",
             s->name, &s->fileofs, &s->base, &s->limit, &dummy) != 4)
            DIE("Syntax error in line\n%s\n", linebuf);
        if (s->base > s->limit)
            DIE("Segment %s: limit %x below base %x\n",
                s->name, s->limit, s->base);
        segs++;
    }
}

typedef struct {
    unsigned char data[1024];
    unsigned len;
    unsigned readidx;
    unsigned char type;
} record_t;

int read_record(FILE *obj, record_t *r)
{
    int chksum_byte;
    unsigned char buf[3];
    if (fread(buf, 1, 3, obj) != 3)
        return 0;
    r->type = buf[0];
    r->len = buf[1] | (buf[2] << 8);
    if (r->len == 0)
        DIE("malformed object record - doesn't have a checksum\n");
    r->len--;
    if (r->len > 1024)
        DIE("malformed object record - size %d exceeds 1K\n", r->len);
    if (fread(r->data, 1, r->len, obj) != r->len)
        DIE("unexpected end of file reading object record\n");
    chksum_byte = getc(obj);
    if (chksum_byte == EOF)
        DIE("unexpected end of file reading object record checksum\n");
    if (chksum_byte != 0)
    {
        unsigned i;
        for (i = 0; i < r->len; i++)
            chksum_byte += r->data[i];
        for (i = 0; i < 3; i++)
            chksum_byte += buf[i];
        if ((chksum_byte & 0xFF) != 0)
            DIE("checksum error in object file record");
    }
    r->readidx = 0;
    return 1;
}

#define len_remaining(r) ((r)->len - (r)->readidx)
#define at_end(r) (len_remaining(r) == 0)

unsigned char getc_or_die(record_t *r)
{
    if (at_end(r))
        DIE("unexpected end of record\n");
    return r->data[r->readidx++];
}

unsigned getw_or_die(record_t *r)
{
    unsigned temp = getc_or_die(r);
    return temp | (getc_or_die(r) << 8);
}

unsigned get_idx(record_t *r)
{
    unsigned temp = getc_or_die(r);
    if (temp & 0x80)
        return ((temp & 0x7F) << 8) | getc_or_die(r);
    else
        return temp;
}

void record_read(void *target, size_t len, record_t *r)
{
    if (len_remaining(r) < len)
        DIE("unexpected end of record trying to read %d bytes\n", len);
    memcpy(target, &r->data[r->readidx], len);
    r->readidx += len;
}

void record_copyto(FILE *target, size_t len, record_t *r)
{
    if (len_remaining(r) < len)
        DIE("unexpected record end in copy\n");
    if (fwrite(&r->data[r->readidx], 1, len, target) != len)
       DIE("data write error\n");
    r->readidx += len;
}

void assign_nameidx(const char *name, int idx)
{
    int s;
    for (s = 0; s < segs; s++)
        if (strcmp(name, segtable[s].name) == 0)
            segtable[s].nameidx = idx;
}

seg_t *seg_by_nameidx(int nameidx)
{
    int s;
    for (s = 0; s < segs; s++)
        if (segtable[s].nameidx == nameidx)
            return &segtable[s];
    return NULL;
}

seg_t *seg_by_segidx(int segidx)
{
    int s;
    for (s = 0; s < segs; s++)
        if (segtable[s].segidx == segidx)
            return &segtable[s];
    return NULL;
}

void handle_names(record_t *r)
{
    char namebuf[256];
    while (!at_end(r))
    {
        unsigned namelen = getc_or_die(r);
        record_read(namebuf, namelen, r);
        namebuf[namelen] = 0;
        nameidx++;
        if (dbg)
           printf("Name %d: %s\n", nameidx, namebuf);
        assign_nameidx(namebuf, nameidx);
    }
}

void handle_segment(record_t *r)
{
    unsigned char type_byte = getc_or_die(r);
    unsigned seglimit;
    seg_t *s;
    segidx++;
    if ((type_byte & 0xE0) == 0)
        // 'AT' segment, defines stuff already present
        return;
    seglimit = (getw_or_die(r) - 1) & 0xFFFF;
    s = seg_by_nameidx(get_idx(r));
    if (s)
    {
        s->segidx = segidx;
        if (dbg) printf("Segment %s has index %d\n",
                        s->name, s->segidx);
        if (s->limit < seglimit)
            DIE("Segment %s overflow (maplimit = %04x, objlimit = %04x\n",
                s->name, s->limit, seglimit);
    }
    else
    {
        // we don't store names of segments not in the map,
        // so we can't tell the user the name of the segment
        fprintf(stderr, "WARNING: ignoring a non-AT segment not in the map\n");
    }
}

void handle_fixupp(record_t *r)
{
    while(!at_end(r))
    {
        unsigned char headbyte = getc_or_die(r);
        if ((headbyte & 0x80) == 0)
        {
            // "thread": common frame or target
            if ((headbyte & 0x1C) == 0)  // T0 or F0
                get_idx(r);
            else if((headbyte & 0x58) == 0x50) // F4 or F4
                ; // no-index types
            else
                DIE("FIXUPP thread type %02x unsupported\n", headbyte);
        }
        else
        {
            // actual fixup
            getc_or_die(r);  // skip low location byte
            if ((headbyte & 0x7C) == 0x44)
            {
                // segment relative 16-bit offset fixup
                int has_framedatum = 0;
                int has_targetdatum = 0;
                unsigned char fixdata = getc_or_die(r);
                if ((fixdata & 0x80) == 0)
                {
                    // explit F type
                    if ((fixdata & 0x70) == 0)
                        has_framedatum = 1;   // relative to explicit segment
                    else if ((fixdata & 0x60) == 0x40)
                        ;                     // frame is implicit
                    else
                        DIE("unsupported frame type %d\n", fixdata >> 4);
                }
                // threaded frames are always OK (thread would have been
                // rejected on unhandled types) and never have frame datum
                if ((fixdata & 8) == 0)
                {
                    // explicit T type
                    if ((fixdata & 3) != 0)
                        DIE("unsupported target type %d\n", fixdata & 3);
                    has_targetdatum = 1;
                }
                if (has_framedatum) get_idx(r);
                if (has_targetdatum) get_idx(r);
                if ((fixdata & 4) == 0)
                {
                    unsigned displacement = getw_or_die(r);
                    if (displacement != 0)
                        DIE("non-zero displacement %x\n", displacement);
                }
            }
            else
                DIE("FIXUPP fixup type %02x unsupported\n", headbyte);
        }
    }
}

void handle_ledata(record_t *r, FILE *bin)
{
    unsigned ofs;
    unsigned segidx;
    seg_t *seg;
    segidx = get_idx(r);
    ofs = getw_or_die(r);
    if (!at_end(r))
    {
        unsigned len = len_remaining(r);
        seg = seg_by_segidx(segidx);
        if (!seg)
            DIE("LEDATA in missing section");
        if (ofs < seg->base)
            DIE("LEDATA for segment %s at %04x below base (%04x)",
                seg->name, ofs, seg->base);
        if (((ofs + len - 1) & 0xFFFF) < ofs)
            DIE("LEDATA for segment %s with start %04x, len %04x"
                " overflows 64K\n",
                seg->name, ofs, len);
        if (ofs + len - 1 > seg->limit)
            DIE("LEDATA for segment %s at %04x, len %04x exceeds"
                " segment limit %04x\n",
                seg->name, ofs, len, seg->limit);
        fseek(bin, seg->fileofs + ofs - seg->base, SEEK_SET);
        record_copyto(bin, len, r);
    }
}

void expand_lidata(record_t *r, FILE *bin, unsigned *maxexpand)
{
    unsigned int repeat_count = getw_or_die(r);
    unsigned int item_count = getw_or_die(r);
    unsigned int i;
    unsigned mark = r->readidx;
    while (repeat_count--)
    {
        r->readidx = mark;
        if (!item_count)
        {
            unsigned char blobsize = getc_or_die(r);
            if (blobsize > *maxexpand)
                DIE("segment limit exceeded in LIDATA expansion");
            *maxexpand -= blobsize;
            // blob payload
            record_copyto(bin, blobsize, r);
        }
        else
            for (i = 0; i < item_count; i++)
                expand_lidata(r, bin, maxexpand);
    }
}

void skip_lidata(record_t *r)
{
    unsigned int item_count;
    getw_or_die(r);     // ignore repeat count
    item_count = getw_or_die(r);
    if (!item_count)
    {
        // blob payload
        unsigned char bloblen = getc_or_die(r);
        while(bloblen--)
            if (getc_or_die(r) != 0)
               DIE("non-NUL LIDATA in nonpresent section");
    }
    else
    {
        while (item_count--)
            skip_lidata(r);
    }
}

void handle_lidata(record_t *r, FILE *bin)
{
    unsigned ofs;
    unsigned segidx;
    unsigned maxexpand;
    seg_t *seg;
    segidx = get_idx(r);
    ofs = getw_or_die(r);
    if (!at_end(r))
    {
        seg = seg_by_segidx(segidx);
        if (seg)
        {
            if(ofs < seg->base)
                DIE("LIDATA for segment %s at %04x below base (%04x)",
                    seg->name, ofs, seg->base);
            if(ofs > seg->limit)
                DIE("LIDATA for segment %s at %04x above limit (%04x)",
                    seg->name, ofs, seg->limit);
            fseek(bin, seg->fileofs + ofs - seg->base, SEEK_SET);
            maxexpand = seg->limit - ofs;
            expand_lidata(r, bin, &maxexpand);
        }
        else
            skip_lidata(r);
    }
}

void fix_checksum(chks_t *dtor, FILE *bin)
{
    long current = dtor->firstofs;
    long bytes = dtor->lastofs - current + 1;
    fseek(bin, current, SEEK_SET);
    switch(dtor->type)
    {
        case CHKS_SUM8:
        {
            unsigned char sum = 0;
            while (bytes--)
            {
                int next = getc(bin);
                if (next == EOF)
                    DIE("can't read data for checksum\n");
                if (current == dtor->sumofs)  // mask byte to be overwritten
                    next = 0;
                sum += (unsigned char)next;
                current++;
            }
            fseek(bin, dtor->sumofs, SEEK_SET);
            putc((unsigned char)-sum, bin);
        }
    }
}

void apply_patch(FILE *bin, FILE *obj)
{
    int c;
    record_t r;
    if (!read_record(obj, &r) || r.type != 0x80)
        DIE("patch file doesn't start with a THEADR record\n");
    while (read_record(obj, &r))
    {
        switch (r.type)
        {
        case 0x88:   // comment record
            break;
        case 0x8A:
            goto done;
        case 0x96:
            handle_names(&r);
            break;
        case 0x98:
            handle_segment(&r);
            break;
        case 0x9C:
            handle_fixupp(&r);
            break;
        case 0xa0:
            handle_ledata(&r, bin);
            break;
        case 0xa2:
            handle_lidata(&r, bin);
            break;
        default:
            fprintf(stderr, "WARNING: unhandle record %02x, size %d\n",
                    r.type, r.len);
            break;
        }
    }
    fprintf(stderr, "WARNING: no end of module record encountered\n");
done:
    for (c = 0; c < chks; c++)
        fix_checksum(checksums + c, bin);
}

int main(int argc, char *argv[])
{
    FILE *binary;
    FILE *obj;
    if (argc != 4)
        DIE("omfpatch <bin> <seglist> <obj>\n");
    binary = fopen(argv[1], "r+b");
    if (!binary)
        DIE("Can't open binary %s for patching\n", argv[1]);
    obj = fopen(argv[3], "rb");
    if (!obj)
        DIE("Can't open object file %s\n", argv[3]);
    load_segs(argv[2]);
    apply_patch(binary, obj);
    printf("patched %s using %s - OK\n", argv[1], argv[3]);
    return 0;
}