// 
//  apt.c
//  src/jbloader/helper/apt.c
//  
//  Created 07/05/2023
//  jbloader (helper)
//

#include <jbloader.h>

#define APT_BIN_ROOTFUL "/usr/bin/apt-get"
#define APT_BIN_ROOTLESS "/var/jb/usr/bin/apt-get"
#define DPKG_BIN_ROOTFUL "/usr/bin/dpkg"
#define DPKG_BIN_ROOTLESS "/var/jb/usr/bin/dpkg"

#define SOURCES_PATH_ROOTFUL "/etc/apt/sources.list.d/palera1n.sources"
#define SOURCES_PATH_ROOTLESS "/var/jb/etc/apt/sources.list.d/palera1n.sources"
#define ZEBRA_PATH "/var/mobile/Library/Application Support/xyz.willy.Zebra"

#define PROCURSUS_PATH "/etc/apt/sources.list.d/procursus.sources"

int apt(char* args[]) {
    int ret, status;
    pid_t pid;

    const char *apt = check_rootful() ? APT_BIN_ROOTFUL : APT_BIN_ROOTLESS;
    if (access(apt, F_OK) != 0) {
        fprintf(stderr, "%s %s %s%d%s\n", "Unable to access apt:", strerror(errno), "(", errno, ")");
        return EACCES;
    }

    ret = posix_spawnp(&pid, apt, NULL, NULL, args, NULL);
    if (ret != 0) {
        fprintf(stderr, "%s %d\n", "apt failed with error:", ret);
        return ret;
    }

    waitpid(pid, &status, 0);
    return 0;
}
            
int upgrade_packages() {
    int installed = pm_installed();
    if (installed == 0) {
        fprintf(stderr, "%s\n", "No package manager found, unable to continue.");
        return -1;
    }

    apt((char*[]){"apt-get", "update", "--allow-insecure-repositories", NULL});
    apt((char*[]){"apt-get", "-o", "Dpkg::Options::=--force-confnew", "-y", "--fix-broken",  "install", "--allow-unauthenticated", NULL});
    apt((char*[]){"apt-get", "-o", "Dpkg::Options::=--force-confnew", "-y", "upgrade", "--allow-unauthenticated", NULL});

    return 0;
}


int additional_packages(const char *package_data) {
    int installed = pm_installed();
    if (installed == 0) {
        fprintf(stderr, "%s\n", "No package manager found, unable to continue.");
        return -1;
    }

    apt((char*[]){"apt-get", "-o", "Dpkg::Options::=--force-confnew", "install", package_data, "-y", "--allow-unauthenticated", NULL});

    return 0;
}

int add_sources_apt(const char *repos_data) {
    const char *sources_file = check_rootful() ? SOURCES_PATH_ROOTFUL : SOURCES_PATH_ROOTLESS;
    FILE *apt_sources = fopen(sources_file, "w+");
    if (apt_sources == NULL) {
        fprintf(stderr, "Failed to open sources file: %s\n", sources_file);
        return -1;
    }
    
    fputs(repos_data, apt_sources);
    
    int ret = fclose(apt_sources);
    if (ret != 0) {
        fprintf(stderr, "Failed to close apt sources file: %d\n", ret);
        return ret;
    }

    return 0;
}

int add_sources(const char *repos_data) {
    int installed = pm_installed();
    int ret;

    ret = add_sources_apt(repos_data);
    if (ret != 0) {
        fprintf(stderr, "%s %d\n", "Failed to add sources to apt:", ret);
        return ret;
    }

    remove(ZEBRA_PATH);

    ret = upgrade_packages();
    if (ret != 0) {
        fprintf(stderr, "%s %d\n", "Failed to update packages via apt:", ret);
        return ret;
    }

    return 0;
}

int add_packages(const char *package_data) {
    int installed = pm_installed();
    int ret;
    ret = additional_packages(package_data);
    if (ret != 0) {
        fprintf(stderr, "%s %d\n", "Failed to install additional packages via apt:", ret);
        return ret;
    }
    
    return 0;
}
