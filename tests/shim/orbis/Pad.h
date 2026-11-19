typedef struct { unsigned buttons; char pad[256]; } OrbisPadData;
int scePadInit(void); int scePadOpen(int, int, int, void *); int scePadReadState(int, OrbisPadData *);
