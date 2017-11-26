#define BUFFER_SIZE 1024

char *OS_Name(char *bytes);
char *volume_label(char *bytes);
int total_disk_space(char *bytes);
int free_disk_space(char *bytes);
int num_files_root_dir(char *bytes);
int num_FAT_cop(char *bytes);
int sect_per_FAT(char *bytes);
