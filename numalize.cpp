#include <iostream>
#include <sstream>
#include <unordered_map>
#include <map>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <tuple>
#include <unistd.h>

#include <sys/time.h>
#include <sys/resource.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <libelf/libelf.h>
#include <libelf/gelf.h>
#include <execinfo.h>


#include "pin.H"

#define REAL_TID(tid) ((tid)>=2 ? (tid)-1 : (tid))
#define ACC_T_READ 0
#define ACC_T_WRITE 1

char TYPE_NAME[2]={'R','W'};

const int MAXTHREADS = 1024;
int PAGESIZE;
unsigned int REAL_PAGESIZE;

KNOB<int> COMMSIZE(KNOB_MODE_WRITEONCE, "pintool", "cs", "6", "comm shift in bits");
KNOB<int> INTERVAL(KNOB_MODE_WRITEONCE, "pintool", "i", "0", "print interval (ms) (0=disable)");

KNOB<bool> DOCOMM(KNOB_MODE_WRITEONCE, "pintool", "c", "0", "enable comm detection");
KNOB<bool> DOPAGE(KNOB_MODE_WRITEONCE, "pintool", "p", "0", "enable page usage detection");


struct{
    string sym;
    ADDRINT sz;
    int ended; // 0 if the malloc is not completed (missing ret val)
} Allocs[MAXTHREADS+1];

ofstream fstructStream;

int num_threads = 0;

UINT64 comm_matrix[MAXTHREADS][MAXTHREADS]; // comm matrix

unordered_map<UINT64, array<UINT32,2>> commmap; // cache line -> list of tids that previously accesses

array<unordered_map<UINT64, UINT64>, MAXTHREADS+1> pagemap[2];
array<unordered_map<UINT64, UINT64>, MAXTHREADS+1> ftmap;

map<UINT32, UINT32> pidmap; // pid -> tid

array<UINT64, MAXTHREADS> stacksize; // stack size of each thread in pages
array<UINT64, MAXTHREADS> stackmax;  // tid -> stack base address from file (unpinned application)
map<UINT32, UINT64> stackmap;        // stack base address from pinned application

string img_name;


    static inline
VOID inc_comm(int a, int b)
{
    // if (a!=b-1)
    comm_matrix[a][b-1]++;
}


VOID do_comm(ADDRINT addr, THREADID tid)
{
    UINT64 line = addr >> COMMSIZE;
    tid = REAL_TID(tid);
    int sh = 1;

    THREADID a = commmap[line][0];
    THREADID b = commmap[line][1];


    if (a == 0 && b == 0)
        sh = 0;
    if (a != 0 && b != 0)
        sh = 2;

    switch (sh) {
        case 0: /* no one accessed line before, store accessing thread in pos 0 */
            commmap[line][0] = tid+1;
            break;

        case 1: /* one previous access => needs to be in pos 0 */
            // if (a != tid+1) {
            inc_comm(tid, a);
            commmap[line][1] = a;
            commmap[line][0] = tid+1;
            // }
            break;

        case 2: // two previous accesses
            // if (a != tid+1 && b != tid+1) {
            inc_comm(tid, a);
            inc_comm(tid, b);
            commmap[line][1] = a;
            commmap[line][0] = tid+1;
            // } else if (a == tid+1) {
            // 	inc_comm(tid, b);
            // } else if (b == tid+1) {
            // 	inc_comm(tid, a);
            // 	commmap[line][1] = a;
            // 	commmap[line][0] = tid+1;
            // }

            break;
    }
}

    static inline
UINT64 get_tsc()
{
#if defined(__i386) || defined(__x86_64__)
    unsigned int lo, hi;
    __asm__ __volatile__ (
            "cpuid \n"
            "rdtsc"
            : "=a"(lo), "=d"(hi) /* outputs */
            : "a"(0)             /* inputs */
            : "%ebx", "%ecx");   /* clobbers*/
    return ((UINT64)lo) | (((UINT64)hi) << 32);
#elif defined(__ia64)
    UINT64 r;
    __asm__ __volatile__ ("mov %0=ar.itc" : "=r" (r) :: "memory");
    return r;
#else
#error "architecture not supported"
#endif
}

VOID do_numa(ADDRINT addr, THREADID tid, ADDRINT type)
{
    UINT64 page = addr >> PAGESIZE;
    tid=REAL_TID(tid);

    if (pagemap[type][tid][page]++ == 0 && pagemap[(type+1)%2][tid][page]==0)
        ftmap[tid][page] = get_tsc();
}


VOID trace_memory_comm(INS ins, VOID *v)
{
    if (INS_IsMemoryRead(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)do_comm, IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_END);

    if (INS_HasMemoryRead2(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)do_comm, IARG_MEMORYREAD2_EA, IARG_THREAD_ID, IARG_END);

    if (INS_IsMemoryWrite(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)do_comm, IARG_MEMORYWRITE_EA, IARG_THREAD_ID, IARG_END);
}

VOID trace_memory_page(INS ins, VOID *v)
{
    if (INS_IsMemoryRead(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)do_numa, IARG_MEMORYREAD_EA, IARG_THREAD_ID,IARG_ADDRINT,ACC_T_READ, IARG_END);

    if (INS_HasMemoryRead2(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)do_numa, IARG_MEMORYREAD2_EA, IARG_THREAD_ID, IARG_ADDRINT,ACC_T_READ,IARG_END);

    if (INS_IsMemoryWrite(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)do_numa, IARG_MEMORYWRITE_EA, IARG_THREAD_ID, IARG_ADDRINT,ACC_T_WRITE, IARG_END);
}


VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    __sync_add_and_fetch(&num_threads, 1);

    if (num_threads>=MAXTHREADS+1) {
        cerr << "ERROR: num_threads (" << num_threads << ") higher than MAXTHREADS (" << MAXTHREADS << ")." << endl;
    }

    int pid = PIN_GetTid();
    pidmap[pid] = tid ? tid - 1 : tid;
    stackmap[pid] = PIN_GetContextReg(ctxt, REG_STACK_PTR) >> PAGESIZE;
}


VOID print_matrix()
{
    static long n = 0;
    ofstream f;
    char fname[255];

    if (INTERVAL)
        sprintf(fname, "%s.%06ld.comm.csv", img_name.c_str(), n++);
    else
        sprintf(fname, "%s.full.comm.csv", img_name.c_str());

    int real_tid[MAXTHREADS+1];
    int i = 0, a, b;

    for (auto it : pidmap)
        real_tid[it.second] = i++;

    cout << fname << endl;

    f.open(fname);

    for (int i = num_threads-1; i>=0; i--) {
        a = real_tid[i];
        for (int j = 0; j<num_threads; j++) {
            b = real_tid[j];
            f << comm_matrix[a][b] + comm_matrix[b][a];
            if (j != num_threads-1)
                f << ",";
        }
        f << endl;
    }
    f << endl;

    f.close();
}


VOID getRealStackBase()
{
    ifstream ifs;
    ifs.open(img_name + ".stackmap");

    string line;
    int tid;
    UINT64 maxaddr, size;

    while (getline(ifs, line)) {
        stringstream lineStream(line);
        lineStream >> tid >> maxaddr >> size;
        stackmax[tid] = maxaddr;
        stacksize[tid] = size;
    }

    ifs.close();
}


void print_numa()
{
    int real_tid[MAXTHREADS+1];

    unordered_map<UINT64, array<UINT64, MAXTHREADS+1>> finalmap[2];
    unordered_map<UINT64, pair<UINT64, UINT32>> finalft;

    int i = 0;

    static long n = 0;
    ofstream f;
    char fname[255];

    if (INTERVAL)
        sprintf(fname, "%s.%06ld.page.csv", img_name.c_str(), n++);
    else
        sprintf(fname, "%s.full.page.csv", img_name.c_str());

    cout << ">>> " << fname << endl;

    getRealStackBase();

    f.open(fname);

    for (auto it : pidmap){
        real_tid[it.second] = i++;
        cout << it.second << "->" << i-1 << " Stack " << stackmap[it.first] << " " << stacksize[i-1] << endl;
    }

    f << "addr,firstacc,type";
    for (int i = 0; i<num_threads; i++)
        f << ",T" << i;
    f << "\n";


    // determine which thread accessed each page first
    for (int tid = 0; tid<num_threads; tid++) {
        for(int i=0; i < 2; ++i){
            for (auto it : pagemap[i][tid]) {
                //TODO
                finalmap[i][it.first][tid] = pagemap[i][tid][it.first];
                if (finalft[it.first].first == 0 || finalft[it.first].first > ftmap[tid][it.first])
                    finalft[it.first] = make_pair(ftmap[tid][it.first], real_tid[tid]);
            }
        }
    }

    // fix stack and print pages to csv
    for(int i=0; i < 2; ++i)
    {
        for(auto it : finalmap[i]) {
            UINT64 pageaddr = it.first;
            f << pageaddr << "," << finalft[it.first].second<< ","<< TYPE_NAME[i] ;

            for (int i=0; i<num_threads; i++)
                f << "," << it.second[real_tid[i]];

            f << "\n";
        }
    }

    f.close();
}


VOID mythread(VOID * arg)
{
    while(!PIN_IsProcessExiting()) {
        PIN_Sleep(INTERVAL ? INTERVAL : 100);

        if (INTERVAL == 0)
            continue;

        if (DOCOMM) {
            print_matrix();
            memset(comm_matrix, 0, sizeof(comm_matrix));
        }
        if (DOPAGE) {
            print_numa();
            // for(auto it : pagemap)
            // 	fill(begin(it.second), end(it.second), 0);
        }
    }
}

//retrieve structures names address and size
int getStructs(const char* file);

string get_struct_name(string str)
{
    // Remove everything after first '='
    string ret=str.substr(0,str.find('='));
    // Keep the last word
    ret=ret.substr(ret.find_last_of(' ')+1);
    //Remove preprending '*' if any
    return ret.substr(ret.find('*')+1);
}

VOID PREMALLOC(ADDRINT retip, THREADID tid, ADDRINT sz)
{
    int col, ln;
    int id=REAL_TID(tid);
    string fname;
    if( (unsigned int) sz >= REAL_PAGESIZE)
    {

        PIN_LockClient();
        PIN_GetSourceLocation 	(retip, &col, &ln, &fname);
        PIN_UnlockClient();
        //TODO: optimize this to read file only once
        ifstream fstr(fname);
        string line;
        if(!fstr)
        {
            cerr << "Can't open file '" << fname << "', malloc will be anonymous"<< endl;
            cerr << "Have you compiled your program with '-g' flag ?" <<endl;
            Allocs[id].sym="AnonymousStruct";
        }
        else
        {
            for(int i=0; i< ln; ++i)
                getline(fstr, line);
            Allocs[id].sym=get_struct_name(line);
        }
        Allocs[id].sz=sz;
        Allocs[id].ended=0;
    }
}
VOID POSTMALLOC(ADDRINT ret, THREADID tid)
{
    int id=REAL_TID(tid);
    static int anonymousId=0;
    if (Allocs[id].ended==0)
    {
        fstructStream << Allocs[id].sym;
        if(Allocs[id].sym.compare("AnonymousStruct")==0)
            fstructStream << anonymousId++;
        fstructStream <<","<<ret<<","<<Allocs[id].sz<<endl;
        Allocs[id].ended=1;
    }
}

VOID binName(IMG img, VOID *v)
{
    if (IMG_IsMainExecutable(img))
    {
        img_name = basename(IMG_Name(img).c_str());
        char fname[255];
        sprintf(fname, "%s.structs.csv", img_name.c_str());
        fstructStream.open(fname);
        fstructStream << "name,start,sz" << endl;

    }
    getStructs(IMG_Name(img).c_str());
    RTN mallocRtn = RTN_FindByName(img, "malloc");
    if (RTN_Valid(mallocRtn))
    {
        RTN_Open(mallocRtn);
        RTN_InsertCall(mallocRtn, IPOINT_BEFORE, (AFUNPTR)PREMALLOC,
                IARG_RETURN_IP, IARG_THREAD_ID,
                IARG_FUNCARG_ENTRYPOINT_VALUE, 0,  IARG_END);
        RTN_InsertCall(mallocRtn, IPOINT_AFTER, (AFUNPTR)POSTMALLOC,
                IARG_FUNCRET_EXITPOINT_VALUE, IARG_THREAD_ID, IARG_END);
        RTN_Close(mallocRtn);
    }
}



VOID Fini(INT32 code, VOID *v)
{
    if (DOCOMM)
        print_matrix();
    if (DOPAGE)
        print_numa();
    fstructStream.close();

    cout << endl << "MAXTHREADS: " << MAXTHREADS << " COMMSIZE: " << COMMSIZE << " PAGESIZE: " << PAGESIZE << " INTERVAL: " << INTERVAL << endl << endl;
}




int main(int argc, char *argv[])
{
    PIN_InitSymbols();
    if (PIN_Init(argc,argv)) return 1;

    REAL_PAGESIZE=sysconf(_SC_PAGESIZE);
    PAGESIZE = log2(REAL_PAGESIZE);

    if (!DOCOMM && !DOPAGE) {
        cerr << "ERROR: need to choose at least one of communication (-c) or page usage (-p) detection" << endl;
        cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
        return 1;
    }

    THREADID t = PIN_SpawnInternalThread(mythread, NULL, 0, NULL);
    if (t!=1)
        cerr << "ERROR internal thread " << t << endl;

    cout << endl << "MAXTHREADS: " << MAXTHREADS << " COMMSIZE: " << COMMSIZE << " PAGESIZE: " << PAGESIZE << " INTERVAL: " << INTERVAL << endl << endl;

    if (DOPAGE)
        INS_AddInstrumentFunction(trace_memory_page, 0);

    if (DOCOMM) {
        INS_AddInstrumentFunction(trace_memory_comm, 0);
        commmap.reserve(100*1000*1000);
    }



    IMG_AddInstrumentFunction(binName, 0);
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();
}

/*
 * The following function is an adaptation of the libelf-howto.c from:
 * http://em386.blogspot.com
 *
 */

#define ERR -1

int getStructs(const char* file)
{
    Elf *elf;                       /* Our Elf pointer for libelf */
    Elf_Scn *scn=NULL;                   /* Section Descriptor */
    Elf_Data *edata=NULL;                /* Data Descriptor */
    GElf_Sym sym;			/* Symbol */
    GElf_Shdr shdr;                 /* Section Header */




    int fd; 		// File Descriptor
    char *base_ptr;		// ptr to our object in memory
    struct stat elf_stats;	// fstat struct
    cout << "Retrieving data structures from file "<< file << endl;

    if((fd = open(file, O_RDONLY)) == ERR)
    {
        cerr << "couldnt open" << file << endl;
        return ERR;
    }

    if((fstat(fd, &elf_stats)))
    {
        cerr << "could not fstat" << file << endl;
        close(fd);
        return ERR;
    }

    if((base_ptr = (char *) malloc(elf_stats.st_size)) == NULL)
    {
        cerr << "could not malloc" << endl;
        close(fd);
        return ERR;
    }

    if((read(fd, base_ptr, elf_stats.st_size)) < elf_stats.st_size)
    {
        cerr << "could not read" << file << endl;
        free(base_ptr);
        close(fd);
        return ERR;
    }

    /* Check libelf version first */
    if(elf_version(EV_CURRENT) == EV_NONE)
    {
        cerr << "WARNING Elf Library is out of date!" << endl;
    }

    elf = elf_begin(fd, ELF_C_READ, NULL);	// Initialize 'elf' pointer to our file descriptor

    elf = elf_begin(fd, ELF_C_READ, NULL);

    int symbol_count;
    int i;

    while((scn = elf_nextscn(elf, scn)) != NULL)
    {
        gelf_getshdr(scn, &shdr);
        // Get the symbol table
        if(shdr.sh_type == SHT_SYMTAB)
        {
            // edata points to our symbol table
            edata = elf_getdata(scn, edata);
            // how many symbols are there? this number comes from the size of
            // the section divided by the entry size
            symbol_count = shdr.sh_size / shdr.sh_entsize;
            // loop through to grab all symbols
            for(i = 0; i < symbol_count; i++)
            {
                // libelf grabs the symbol data using gelf_getsym()
                gelf_getsym(edata, i, &sym);
                // Keep only objects big enough to be data structures
                if(ELF32_ST_TYPE(sym.st_info)==STT_OBJECT &&
                        sym.st_size >= REAL_PAGESIZE)
                {
                    fstructStream << elf_strptr(elf, shdr.sh_link, sym.st_name) <<
                        "," << sym.st_value << "," << sym.st_size << endl;
                }
            }
        }
    }
    return 0;
}
