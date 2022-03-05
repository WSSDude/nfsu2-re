/**
Instructs idcparse.c to parse the IDC file and then uses that to make
html documentation for the structs/enums/functions/data.
*/

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if 0
#define FREE(X) free(X)
#else
#define FREE(X)
#endif

#define IDCP_VERBOSE_LEVEL 0
#include "idcparse.c"

/*These definitions are specific to your IDA's version.
See <IDA path>/idc/idc.idc (without the IDC_ prefix in the definitions' names)*/
#define IDC_FF_STRUCT 0x60000000
#define IDC_DT_TYPE   0xF0000000
#define IDC_MS_CLS    0x00000600
#define IDC_FF_DATA   0x00000400
#define MS_0TYPE      0x00F00000
#define FF_0ENUM      0x00800000
#define IDC_is_data(F) ((F & IDC_MS_CLS) == IDC_FF_DATA)
#define IDC_is_struct(F) (IDC_is_data(F) && (F & IDC_DT_TYPE) == IDC_FF_STRUCT)
#define IDC_is_enum(F) ((F & MS_0TYPE) == FF_0ENUM)

#define IDCPATH "../../nfsu2-re-idc/SPEED2.idc"

struct docgen_structinfo {
	struct idcp_struct *struc;
	int name_len;
};

struct docgen_funcinfo {
	struct idcp_stuff *stuff;
	int name_len;
};

struct docgen {
	struct idcparse *idcp;
	int num_structinfos;
	struct docgen_structinfo *structinfos;
	int num_funcinfos;
	struct docgen_funcinfo *funcinfos;
};
/*jeanine:p:i:20;p:0;a:b;x:-104.54;y:12.48;*/

static
struct docgen_structinfo* docgen_find_struct(struct docgen *dg, char *name)
{
	struct docgen_structinfo *structinfo;
	int i, len;

	len = strlen(name);
	for (i = 0, structinfo = dg->structinfos; i < dg->num_structinfos; i++, structinfo++) {
		if (len == structinfo->name_len && !strcmp(name, structinfo->struc->name)) {
			return structinfo;
		}
	}
	return NULL;
}

static
int docgen_get_struct_size(struct idcparse *idcp, struct idcp_struct *struc)
{
	struct idcp_struct_member *mem;

	if (struc->end_idx > struc->start_idx) {
		mem = idcp->struct_members + struc->end_idx - 1;
		return mem->offset + mem->nbytes;
	} else {
		return 0;
	}
}

static
int docgen_stricmp(char *a, char *b)
{
	register char d;
	char i, j;

nchr:
	i = *a;
	j = *b;
	if ('A' <= i && i <= 'Z') {
		i |= 0x20;
	}
	if ('A' <= j && j <= 'Z') {
		j |= 0x20;
	}
	d = i - j;
	if (d) {
		return d;
	}
	if (!i) {
		return -1;
	}
	if (!j) {
		return 1;
	}
	a++;
	b++;
	goto nchr;
}
/*jeanine:p:i:14;p:0;a:b;y:12.13;*/
struct docgen_tmpbuf {
	char used;
	char *data;
	int size;
};

/**
This will come bite me in my ass at some point.
*/
static
struct docgen_tmpbuf* docgen_get_tmpbuf(int minsize)
{
#define DOCGEN_MAX_TMPBUFS 20
	static struct docgen_tmpbuf bufs[DOCGEN_MAX_TMPBUFS];

	struct docgen_tmpbuf *b;
	int i;

	if (minsize < 20000) {
		minsize = 20000;
	}
	/*First try to find an already allocated one.*/
	for (i = 0, b = bufs; i < DOCGEN_MAX_TMPBUFS; i++, b++) {
		if (!b->used && b->size >= minsize) {
			b->used = 1;
			b->data[0] = 0;
			return b;
		}
	}
	/*If not, allocate a free one*/
	for (i = 0, b = bufs; i < DOCGEN_MAX_TMPBUFS; i++, b++) {
		if (!b->used && !b->size) {
			b->used = 1;
			b->size = minsize;
			b->data = malloc(minsize);
			assert(b->data);
			b->data[0] = 0;
			return b;
		}
	}
	/*Realloc a free one*/
	for (i = 0, b = bufs; i < DOCGEN_MAX_TMPBUFS; i++, b++) {
		if (!b->used) {
			b->used = 1;
			b->size = minsize;
			b->data = realloc(b->data, minsize);
			assert(b->data);
			b->data[0] = 0;
			return b;
		}
	}
	assert(((void) "failed to allocate tmpbuf, try increasing DOCGEN_MAX_TMPBUFS", 0));
}

static
void docgen_free_tmpbuf(struct docgen_tmpbuf *buf)
{
	buf->used = 0;
}
/*jeanine:p:i:12;p:11;a:r;x:39.37;y:-68.98;*/
/**
@param tmpbuf might get swapped with a different buffer
@return nonzero if there were unknown structs
*/
/*TODO: enum refs*/
static
int docgen_link_structs(struct docgen *dg, struct docgen_tmpbuf **tmpbuf)
{
	char *bufp, *sub, *end, *strp, c, structname[200], *sn, hasunknown;
	struct docgen_tmpbuf *newbuf, *original;

	hasunknown = 0;
	original = *tmpbuf;
	sub = strstr(original->data, "struct ");
	if (sub) {
		newbuf = docgen_get_tmpbuf(original->size);
		bufp = newbuf->data;
		strp = original->data;
		do {
			/*Copy segment before struct*/
			memcpy(bufp, strp, sub - strp);
			bufp += sub - strp;
			/*Get struct name*/
			end = sub + 6;
			sn = structname - 1;
			do {
				end++; sn++;
				*sn = c = *end;
			} while (c != ' ' && c != ')' && c != '*' && c != '[');
			*sn = 0;
			if (structname[0] != '#' && docgen_find_struct(dg, structname)) {
				bufp += sprintf(bufp, "<a href='structs.html#struc_%s'>struct %s</a>", structname, structname);
			} else {
				hasunknown = 1;
				bufp += sprintf(bufp, "<strong>struct %s</strong>", structname);
			}
			strp = end;
			sub = strstr(strp, "struct ");
		} while (sub);
		*bufp = 0;
		strcat(bufp, strp);
		*tmpbuf = newbuf;
		docgen_free_tmpbuf(original);
		return hasunknown;
	}
	return 0;
}
/*jeanine:p:i:1;p:7;a:r;x:8.89;y:-49.00;*/
static
void docgen_parseidc(struct idcparse *idcp)
{
	FILE *in;
	char *buf;
	int size;

	in = fopen(IDCPATH, "rb");
	assert(((void)"failed to open file "IDCPATH" for reading", in));
	fseek(in, 0l, SEEK_END);
	size = ftell(in);
	rewind(in);
	buf = (char*) malloc(size);
	assert(buf);
	fread(buf, size, 1, in);
	fclose(in);
	if (size > 2 && buf[0] == (char) 0xEF) {
		/*assume UTF8 BOM*/
		idcparse(idcp, buf + 3, size - 3);
	} else {
		idcparse(idcp, buf, size);
	}
	FREE(buf);
}
/*jeanine:p:i:8;p:7;a:r;x:8.89;y:-23.00;*/
static
char* docgen_readcss()
{
	FILE *in;
	char *buf, *a, *b, *end;
	register char c;
	int size;

	in = fopen("style.css", "rb");
	assert(((void)"failed to open file style.css for reading", in));
	fseek(in, 0l, SEEK_END);
	size = ftell(in);
	rewind(in);
	buf = (char*) malloc(size);
	assert(buf);
	fread(buf, size, 1, in);
	a = b = buf;
	end = buf + size;
	while (b != end) {
		c = *b;
		if (c != '\n' && c != '\t') {
			if (!c) {
				break;
			}
			*a = c;
			a++;
		}
		b++;
	}
	*a = 0;
	fclose(in);
	return buf;
}
/*jeanine:p:i:15;p:7;a:r;x:8.89;y:-2.00;*/
static
int docgen_func_sort_compar(const void *_a, const void *_b)
{
	register const struct idcp_stuff *a = ((struct docgen_funcinfo*) _a)->stuff;
	register const struct idcp_stuff *b = ((struct docgen_funcinfo*) _b)->stuff;

	return docgen_stricmp(a->data.func.name, b->data.func.name);
}
/*jeanine:p:i:16;p:19;a:r;x:3.33;*/
/**
@return length or zero if friendlyname is no different
*/
static
int docgen_get_func_friendlyname(char *dest, char *name)
{
	int len;
	char c;

	len = -1;
	do {
		c = *(name++);
		if (c == ':' || c == '?' || c == '.') {
			c = '_';
		}
		*(dest++) = c;
		len++;
	} while (c);
	return len;
}
/*jeanine:p:i:19;p:17;a:r;x:44.11;y:-62.81;*/
/**
@param tmpbuf might get swapped with a different buffer
*/
static
void docgen_gen_func_signature(struct docgen_tmpbuf **signaturebuf, struct docgen *dg, struct idcp_stuff *func)
{
	char *originalname, friendlyname[200], *namepos, *coloncolon, *b, *originalsignature, classname[200];
	int name_len, len, classname_len;
	struct docgen_tmpbuf *newbuf;

	if (!func->data.func.type) {
		sprintf((*signaturebuf)->data, "<h3>%s</h3>", func->data.func.name);
		return;
	}

	/*Function names like 'clazz::func?' get 'friendly' names like 'clazz__func_' in their signatures.*/
	originalsignature = func->data.func.type;
	originalname = func->data.func.name;
	name_len = docgen_get_func_friendlyname(friendlyname, originalname);/*jeanine:r:i:16;*/
	namepos = strstr(originalsignature, friendlyname);
	if (!namepos) {
		fprintf(stderr, "cannot find func friendlyname '%s' (original '%s') in signature '%s'\n",
			friendlyname,
			originalname,
			originalsignature
		);
		assert(0);
	}
	newbuf = docgen_get_tmpbuf((*signaturebuf)->size);
	b = newbuf->data;
	/*Copy part until start of name*/
	len = namepos - originalsignature;
	memcpy(b, originalsignature, len);
	b += len;
	/*Copy and format name*/
	b += sprintf(b, "<h3>");
	coloncolon = strstr(originalname, "::");
	if (coloncolon) {
		/*Find the struct class and link it*/
		classname_len = coloncolon - originalname;
		memcpy(classname, originalname, classname_len);
		classname[classname_len] = 0;
		if (docgen_find_struct(dg, classname)) {
			b += sprintf(b, "<a href='structs.html#struc_%s'>%s</a>", classname, classname);
		} else {
			printf("warn: cannot find struct '%s' for func %X '%s'\n", classname, func->addr, originalname);
			b += sprintf(b, "<strong>%s</strong>", classname);
		}
		b += sprintf(b, "%s", coloncolon);
	} else {
		b += sprintf(b, "%s", originalname);
	}
	b += sprintf(b, "</h3>");
	/*Copy rest of signature*/
	strcpy(b, namepos + name_len);
	docgen_free_tmpbuf(*signaturebuf);
	*signaturebuf = newbuf;

	if (docgen_link_structs(dg, signaturebuf)) {/*jeanine:s:a:r;i:12;*/
		printf("warn: func '%X %s' references an unknown struct\n", func->addr, func->data.func.name);
	}
}
/*jeanine:p:i:17;p:7;a:r;x:8.89;y:-6.00;*/
static
void docgen_print_func(FILE *f, struct docgen *dg, struct idcp_stuff *func)
{
	struct docgen_tmpbuf *signaturebuf;

	signaturebuf = docgen_get_tmpbuf(10000);
	docgen_gen_func_signature(&signaturebuf, dg, func);/*jeanine:r:i:19;*/
	fprintf(f, "<pre id='%X'><i>%X</i> %s</pre>", func->addr, func->addr, signaturebuf->data);
	docgen_free_tmpbuf(signaturebuf);

	if (func->comment) {
		/*TODO: mmparse*/
		fprintf(f, "<p>%s</p>\n", func->comment);
	}
	if (func->rep_comment) {
		/*TODO: mmparse*/
		fprintf(f, "<p>%s</p>\n", func->rep_comment);
	}
}
/*jeanine:p:i:4;p:7;a:r;x:8.89;y:1.00;*/
static
int docgen_struct_sort_compar(const void *_a, const void *_b)
{
	register const struct idcp_struct *a = ((struct docgen_structinfo*) _a)->struc;
	register const struct idcp_struct *b = ((struct docgen_structinfo*) _b)->struc;

	return docgen_stricmp(a->name, b->name);
}
/*jeanine:p:i:9;p:11;a:r;x:39.11;y:-13.00;*/
static
void docgen_format_struct_member_typeandname_when_enum(char *buf, struct idcparse *idcp, struct idcp_struct_member *mem)
{
	/*TODO: what if it's an array*/

	struct idcp_enum *en;

	assert(0 <= mem->typeid && mem->typeid < idcp->num_enums);
	en = idcp->enums + mem->typeid;
	buf += sprintf(buf, "<a href='enums.html#enum_%s'>enum %s</a> %s", en->name, en->name, mem->name);
}
/*jeanine:p:i:13;p:11;a:r;x:38.67;y:-0.50;*/
static
void docgen_format_struct_member_typeandname_when_struct(char *buf, struct idcparse *idcp, struct idcp_struct_member *mem)
{
	/*For a member that has a struct type (determined by 'flags'), 'typeid' can be used to resolve the
	struct, then use 'nbytes' div by the struct's size to determine array size. The member type
	will have the struct name, but that will not show as an array.*/

	struct idcp_struct *struc;
	int struc_size;

	assert(0 <= mem->typeid && mem->typeid < idcp->num_structs);
	struc = idcp->structs + mem->typeid;
	buf += sprintf(buf, "<a href='#struc_%s'>struct %s</a> %s", struc->name, struc->name, mem->name);
	/*member struct type can't be an empty struct*/
	assert((struc_size = docgen_get_struct_size(idcp, struc)));
	if (mem->nbytes > struc_size) {
		assert(!(mem->nbytes % struc_size));
		sprintf(buf, "[%d]", mem->nbytes / struc_size);
	}
}
/*jeanine:p:i:10;p:11;a:r;x:37.78;y:22.25;*/
static
void docgen_format_struct_member_typeandname_when_type(char *buf, struct idcp_struct_member *mem)
{
	/*Setting a struct member type in IDA to 'void *engageMarkers[128]' will show up as
	type 'void *[128]' in the IDC file.*/
	/*Similary, 'int (__thiscall *proc)(int)' shows as 'int (__thiscall *)(int)'.*/

	char *type, *sub, c;
	register int len;

	type = mem->type;
	len = strlen(type);
	if (type[len - 1] == '*') {
		sprintf(buf, "%s%s", mem->type, mem->name);
	} else if (type[len - 1] == ']') {
		buf[0] = 0;
		sub = type + len - 1;
		for (;;) {
			c = *sub;
			if (c == '[') {
				c = *(sub - 1);
				if (c != ']') {
					break;
				}
				sub -= 2;
			} else {
				sub--;
			}
		}
		len = sub - type;
		memcpy(buf, type, len);
		if (c == '*') {
			/*int *[32]*/
			sprintf(buf + len, "%s", mem->name);
		} else {
			/*int[32]*/
			sprintf(buf + len, " %s", mem->name);
		}
		strcat(buf, sub);
	} else if (strstr(type, "(__") && (sub = strstr(type, "*)("))) {
		len = sub - type + 1;
		memcpy(buf, type, len);
		sprintf(buf + len, "%s", mem->name);
		strcat(buf, sub + 1);
	} else {
		sprintf(buf, "%s %s", mem->type, mem->name);
		return;
	}
}
/*jeanine:p:i:11;p:5;a:r;x:3.33;*/
static
void docgen_print_struct_member(FILE *f, struct docgen *dg, struct idcp_struct *struc, int offset, int size, struct idcp_struct_member *mem)
{
	int offsetstrlen, isplaceholder, member_struc_size;
	struct docgen_tmpbuf *typeandname;
	struct idcp_struct *member_struc;
	char offsetstr[16];

	memset(offsetstr, ' ', 8);
	offsetstrlen = sprintf(offsetstr, "/*%X*/", offset);
	if (offsetstrlen < 8) {
		offsetstr[offsetstrlen] = '\t';
		offsetstr[offsetstrlen+1] = 0;
	}

	typeandname = docgen_get_tmpbuf(10000);
	if (mem) {
		isplaceholder = !strncmp(mem->name, "field_", 6) || !strncmp(mem->name, "floatField_", 11);
		if (IDC_is_enum(mem->flag)) {
			docgen_format_struct_member_typeandname_when_enum(typeandname->data, dg->idcp, mem);/*jeanine:r:i:9;*/
		} else if (IDC_is_struct(mem->flag)) {
			docgen_format_struct_member_typeandname_when_struct(typeandname->data, dg->idcp, mem);/*jeanine:r:i:13;*/
		} else if (mem->type) {
			docgen_format_struct_member_typeandname_when_type(typeandname->data, mem);/*jeanine:r:i:10;*/
			if (docgen_link_structs(dg, &typeandname)) {/*jeanine:r:i:12;*/
				printf("warn: struct '%s' member '%s' references an unknown struct\n", struc->name, mem->name);
			}
		} else {
			switch (mem->nbytes) {
			case 1:
				sprintf(typeandname->data, "char %s", mem->name);
				break;
			case 2:
				sprintf(typeandname->data, "short %s", mem->name);
				break;
			case 4:
				sprintf(typeandname->data, "int %s", mem->name);
				break;
			default:
				sprintf(typeandname->data, "char %s[%d]", mem->name, mem->nbytes);
				break;
			};
			if (!isplaceholder) {
				strcat(typeandname->data, " <em>/*unk type*/</em>");
			}
		}
		assert(!mem->comment);
		if (mem->rep_comment) {
			fprintf(f, "<span>/**%s*/</span>\n", mem->rep_comment); /*TODO: mmparse*/
		}
	} else {
		sprintf(typeandname->data, "char _pad%X[0x%X]", offset, size);
		goto yesplaceholder;
	}

	if (isplaceholder) {
yesplaceholder:
		fprintf(f, "<i>%s%s;</i>\n", offsetstr, typeandname->data);
	} else {
		fprintf(f, "<i>%s</i>%s;\n", offsetstr, typeandname->data);
	}
	docgen_free_tmpbuf(typeandname);
}
/*jeanine:p:i:5;p:7;a:r;x:8.89;y:2.00;*/
static
void docgen_print_struct(FILE *f, struct docgen *dg, struct idcp_struct *struc)
{
	int tmp, num_members, size, membersleft, lastoffset, nextoffset;
	struct idcp_struct_member *mem;
	char offsetstr[16];

	num_members = struc->end_idx - struc->start_idx;
	size = docgen_get_struct_size(dg->idcp, struc);

	/*TODO: check if struct name is valid to use as anchor, or replace invalid chars?*/
	fprintf(f, "<pre id='struc_%s'>%s <h3>%s</h3>%s%d%s%X%s",
		struc->name,
		struc->is_union ? "union" : "struct",
		struc->name,
		" { <i>/*",
		num_members,
		" members, size ",
		size,
		"h*/</i>\n"
	);
	mem = dg->idcp->struct_members + struc->start_idx;
	membersleft = struc->end_idx - struc->start_idx;
	lastoffset = 0;
	while (membersleft) {
		if (mem->offset > lastoffset) {
			docgen_print_struct_member(f, dg, struc, lastoffset, mem->offset - lastoffset, NULL);/*jeanine:s:a:r;i:11;*/
		}
		docgen_print_struct_member(f, dg, struc, mem->offset, mem->nbytes, mem);/*jeanine:r:i:11;*/
		lastoffset = mem->offset + mem->nbytes;
		membersleft--;
		mem++;
	}
	fprintf(f, "};%s", "</pre>");
	if (struc->comment) {
		/*TODO: mmparse*/
		fprintf(f, "<p>%s</p>", struc->rep_comment);
	}
	if (struc->rep_comment) {
		/*TODO: mmparse*/
		fprintf(f, "<p>%s</p>", struc->rep_comment);
	}
}
/*jeanine:p:i:7;p:0;a:b;x:116.00;y:12.13;*/
int main(int argc, char **argv)
{
	struct docgen_structinfo *structinfo;
	struct docgen_funcinfo *funcinfo;
	struct idcp_struct **struc;
	struct idcp_stuff **stuff;
	struct idcparse *idcp;
	FILE *f_structs, *f_funcs;
	struct docgen *dg;
	char *css, *name;
	int i, j;

	idcp = malloc(sizeof(struct idcparse));
	assert(((void)"failed to malloc for idcparse", idcp));
	docgen_parseidc(idcp);/*jeanine:r:i:1;*/

	css = docgen_readcss();/*jeanine:r:i:8;*/

	dg = malloc(sizeof(struct docgen));
	assert(((void)"failed to malloc for docgen", dg));
	dg->idcp = idcp;
	/*read functions*/
	dg->funcinfos = malloc(sizeof(struct docgen_funcinfo) * idcp->num_funcs);
	assert(((void)"failed to malloc for dg->funcinfos", dg->funcinfos));
	for (dg->num_funcinfos = 0, j = 0; j < idcp->num_stuffs; j++) {
		if (idcp->stuffs[j].type == IDCP_STUFF_TYPE_FUNC && idcp->stuffs[j].data.func.name) {
			dg->funcinfos[dg->num_funcinfos].stuff = idcp->stuffs + j;
			dg->funcinfos[dg->num_funcinfos].name_len = strlen(idcp->stuffs[j].data.func.name);
			dg->num_funcinfos++;
		}
	}
	qsort((void*) dg->funcinfos, dg->num_funcinfos, sizeof(struct docgen_funcinfo), docgen_func_sort_compar);/*jeanine:r:i:15;*/
	/*read structs*/
	dg->structinfos = malloc(sizeof(struct docgen_structinfo) * idcp->num_structs);
	assert(((void)"failed to malloc for dg->structinfos", dg->structinfos));
	for (i = 0; i < idcp->num_structs; i++) {
		dg->structinfos[i].struc = idcp->structs + i;
		dg->structinfos[i].name_len = strlen(idcp->structs[i].name);
	}
	dg->num_structinfos = idcp->num_structs;
	qsort((void*) dg->structinfos, dg->num_structinfos, sizeof(struct docgen_structinfo), docgen_struct_sort_compar);/*jeanine:r:i:4;*/

	/*funcs*/
	f_funcs = fopen("funcs.html", "wb");
	assert(((void)"failed to open file funcs.html for writing", f_funcs));
	fprintf(f_funcs,
		"%s%s%s",
		"<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'/><title>nfsu2-re/funcs</title><style>",
		css,
		"</style></head><body><header><h1>nfsu2-re</h1><p>link</p></header><nav><p>Home</p></nav>"
	);
	fprintf(f_funcs, "%s%d%s", "<div><h2>Functions (", dg->num_funcinfos, ")</h2><ul>\n");
	for (i = 0, funcinfo = dg->funcinfos; i < dg->num_funcinfos; i++, funcinfo++) {
		fprintf(f_funcs, "<li><a href='#%X'>%s</a></li>\n", funcinfo->stuff->addr, funcinfo->stuff->data.func.name);
		/*TODO: anchor link*/
	}
	fprintf(f_funcs, "%s", "</ul></div><div class='func'>");
	for (i = 0, funcinfo = dg->funcinfos; i < dg->num_funcinfos; i++, funcinfo++) {
		docgen_print_func(f_funcs, dg, funcinfo->stuff);/*jeanine:r:i:17;*/
	}
	fprintf(f_funcs, "%s", "</div></body></html>");
	fclose(f_funcs);

	/*structs*/
	f_structs = fopen("structs.html", "wb");
	assert(((void)"failed to open file structs.html for writing", f_structs));
	fprintf(f_structs,
		"%s%s%s",
		"<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'/><title>nfsu2-re/structs</title><style>",
		css,
		"</style></head><body><header><h1>nfsu2-re</h1><p>link</p></header><nav><p>Home</p></nav>"
	);
	fprintf(f_structs, "%s%d%s", "<div><h2>Structs (", dg->num_structinfos, ")</h2><ul>\n");
	for (i = 0, structinfo = dg->structinfos; i < dg->num_structinfos; i++, structinfo++) {
		name = structinfo->struc->name;
		fprintf(f_structs, "<li><a href='#struc_%s'>%s</a></li>\n", name, name);
		/*TODO: anchor link*/
	}
	fprintf(f_structs, "%s", "</ul></div><div class='struc'>");
	for (i = 0, structinfo = dg->structinfos; i < dg->num_structinfos; i++, structinfo++) {
		docgen_print_struct(f_structs, dg, structinfo->struc);/*jeanine:r:i:5;*/
	}
	fprintf(f_structs, "%s", "</div></body></html>");
	fclose(f_structs);

	return 0;
}
