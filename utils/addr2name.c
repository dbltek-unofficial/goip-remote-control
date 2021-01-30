#if !defined(__MLIBC__) && !defined(__uClinux__)
#include <elf.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 256
#endif

static u_char* get_exec_name(void);
static int create_symtab(int fd);
static int check_sanity(const Elf32_Ehdr* elf_hdr);
static int get_symtab_idx(const Elf32_Shdr* sec_hdr, int num_sects);
static int get_strtab_idx(const Elf32_Shdr* symtab);
static int get_num_syms(const Elf32_Shdr* symtab);
static void fill_symtab(const Elf32_Sym* sym_ent, int num_syms,
    const u_char* strtab);
/*
 * Our global symbol and addresses table.
 */
typedef struct addr2name_st_tag {
    u_char* name; /* symbol name */
    void *begin, *end; /* start and end addresses respectively */
} addr2name_st_t;

addr2name_st_t* addr2name_st = 0;

int addr2name_init(void)
{
    int exec_fd;
    u_char* fname = NULL;

#if 0
	if(get_stack_end()==-1)
		return -1;
#endif

    if ((fname = get_exec_name()) == NULL)
        return -1;

    if ((exec_fd = open(fname, O_RDONLY)) == -1) {
        fprintf(stderr, " couldn't open \"%s\"\n", fname);
        return -1;
    }

    create_symtab(exec_fd);
#if 0		
	if ((create_symtab(exec_fd)) == -1)
		return -1;
#endif
    free(fname);
    close(exec_fd);

    return 0;
}

u_char*
get_exec_name(void)
{
    pid_t pid;
    u_char exe[PATH_MAX], fname[PATH_MAX];

    pid = getpid();

    snprintf(exe, sizeof(exe) - 1, "/proc/%u/exe", (u_int)pid);

    bzero(fname, sizeof(fname));
    if (readlink(exe, fname, sizeof(fname) - 1) == -1) {
        perror("readlink");
        return NULL;
    }

    return strdup(fname);
}

int create_symtab(int fd)
{
    int symtab_idx, strtab_idx, num_syms;
    u_char *contents, *strtab;
    Elf32_Ehdr* elf_hdr;
    Elf32_Shdr* sec_hdr;
    Elf32_Off offset;
    Elf32_Sym* sym_ent;
    struct stat st;

    if (fstat(fd, &st) == -1) {
        perror("fstat");
        return -1;
    }

    contents = (u_char*)mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (contents == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    elf_hdr = (Elf32_Ehdr*)&contents[0];

    if (check_sanity(elf_hdr) == -1)
        return -1;

    sec_hdr = (Elf32_Shdr*)&contents[elf_hdr->e_shoff];

    if ((symtab_idx = get_symtab_idx(sec_hdr, elf_hdr->e_shnum)) == 0) {
        fprintf(stderr, "no symbols were associated to this file\n");
        return -1;
    }

    if ((strtab_idx = get_strtab_idx(&sec_hdr[symtab_idx])) == 0) {
        fprintf(stderr, "the symbol table didn't have an "
                        "associated string table, wtf??\n");
        return -1;
    }

    if (sec_hdr[strtab_idx].sh_type != SHT_STRTAB) {
        fprintf(stderr, "the associated string table for the symbol "
                        "table is not valid!\n");
        return -1;
    }

    offset = sec_hdr[strtab_idx].sh_offset;
    strtab = &contents[offset];

    if ((num_syms = get_num_syms(&sec_hdr[symtab_idx])) == 0) {
        fprintf(stderr, "there were no symbols in the symbol table!\n");
        return -1;
    }

    offset = sec_hdr[symtab_idx].sh_offset;
    sym_ent = (Elf32_Sym*)&contents[offset];

    fill_symtab(sym_ent, num_syms, strtab);

    if (munmap(contents, st.st_size) == -1) {
        perror("munmap");
        return -1;
    }
    return 0;
}

int check_sanity(const Elf32_Ehdr* elf_hdr)
{
    if (memcmp(elf_hdr->e_ident, ELFMAG, SELFMAG) != 0) {
        fprintf(stderr, "not a valid ELF file\n");
        return -1;
    }

    if (elf_hdr->e_ident[EI_CLASS] != ELFCLASS32) {
        fprintf(stderr, "file class not supported\n");
        return -1;
    }

    if (elf_hdr->e_ident[EI_DATA] == ELFDATANONE) {
        fprintf(stderr, "invalid data encoding\n");
        return -1;
    }

    if (elf_hdr->e_ident[EI_VERSION] == EV_NONE) {
        fprintf(stderr, "invalid ELF file version\n");
        return -1;
    }

    if (elf_hdr->e_type != ET_EXEC) {
        fprintf(stderr, "not an executable ELF file\n");
        return -1;
    }

    if (elf_hdr->e_version == EV_NONE) {
        fprintf(stderr, "wrong version number in ELF file\n");
        return -1;
    }

    if (elf_hdr->e_phoff == 0) {
        fprintf(stderr, "program header not found\n");
        return -1;
    }

    if (elf_hdr->e_shoff == 0) {
        fprintf(stderr, "section header not found\n");
        return -1;
    }

    return 0;
}

int get_symtab_idx(const Elf32_Shdr* sec_hdr, int num_sects)
{
    int i;
    int symtab_idx = 0;

    for (i = 0; i < num_sects; i++)
        if (sec_hdr[i].sh_type == SHT_SYMTAB) {
            symtab_idx = i;
            break;
        }

    return symtab_idx;
}

int get_strtab_idx(const Elf32_Shdr* symtab)
{
    return symtab->sh_link;
}

int get_num_syms(const Elf32_Shdr* symtab)
{
    return (symtab->sh_size / symtab->sh_entsize);
}

#define symtab_size sizeof(struct addr2name_st_tag)

void fill_symtab(const Elf32_Sym* sym_ent, int num_syms, const u_char* strtab)
{
    int i, j;

    /*
	 * 1st we need to count how many elements our table will hold and then
	 * we'll allocate enough space.
	 */
    for (i = 1, j = 0; i < num_syms; i++) {
        //fprintf(stderr, "symbol #%d: %s\n", i, strtab + sym_ent[i].st_name);
        if (ELF32_ST_TYPE(sym_ent[i].st_info) == STT_FUNC)
            ++j;
    }

    if ((addr2name_st = (addr2name_st_t*)calloc(j + 1, symtab_size)) == NULL) {
        perror("calloc");
        abort();
    }

    /*
	 * We start with the symbol table entry number one because number 0
	 * (STN_UNDEF) is reserved.
	 */
    for (i = 1, j = 0; i < num_syms; i++) {
        if (ELF32_ST_TYPE(sym_ent[i].st_info) != STT_FUNC)
            continue;
        if (sym_ent[i].st_shndx == 0)
            continue;

        addr2name_st[j].name = strdup(strtab + sym_ent[i].st_name);
        if (addr2name_st[j].name == NULL) {
            perror("strdup");
            abort();
        }
        addr2name_st[j].begin = (void*)sym_ent[i].st_value;
        addr2name_st[j].end = addr2name_st[j].begin + sym_ent[i].st_size;

        //fprintf(stderr, "symbol #%d: %s %p %p\n", i, addr2name_st[j].name, addr2name_st[j].begin, addr2name_st[j].end);

        ++j;
    }

    /*
	 * The table is finished by a void entry.
	 */
}

#undef symtab_size

char* addr_to_name(const void* addr)
{
    int i;

    if (!addr2name_st)
        return NULL;

    for (i = 0; addr2name_st[i].name != NULL; i++)
        if ((addr < addr2name_st[i].end) && (addr >= addr2name_st[i].begin))
            return addr2name_st[i].name;

    return NULL;
}

#else
char* addr_to_name(const void* addr)
{
    return NULL;
}
#endif
