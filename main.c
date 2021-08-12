
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <errno.h>

typedef enum {
	ACTION_MODIFY, ACTION_CREATE
} action_t;

typedef enum {
	TYPE_UNCHANGED = 0,
	TYPE_ZASM, TYPE_AMBER,
	TYPE_NATIVE_C, TYPE_NATIVE_CPP,
} type_t;

#define DEFAULT_PATH "./"

#define DEFAULT_NAME "Untitled Project"
#define DEFAULT_DESCRIPTION "Untitled project which has no title."
#define DEFAULT_VERSION "development"
#define DEFAULT_AUTHOR "Anonymousia de Bergerac-Fleur"
#define DEFAULT_ORGANIZATION "Literally the CIA"

static type_t type = TYPE_UNCHANGED;

static char* path = DEFAULT_PATH;
static char* unique = (char*) 0;

static char* name = (char*) 0;
static char* description = (char*) 0;
static char* version = (char*) 0;
static char* author = (char*) 0;
static char* organization = (char*) 0;

static void write_file(const char* path, const char* value) {
	FILE* file = fopen(path, "wb");
	if (!file) return; // if something goes wrong, too bad, just ignore it

	fprintf(file, "%s", value);
	fclose(file);
}

static uint64_t hash(const char* string) { // djb2 algorithm
	uint64_t hash = 5381;
	
	char c;
	while ((c = *string++)) {
		hash = ((hash << 5) + hash) + c; // hash * 33 + c
	}

	return hash;
}

static int modify(void) {
	printf("[AQUA Manager] Modifying project ...\n");

	if (chdir(path) < 0) {
		fprintf(stderr, "[AQUA Manager] ERROR Project path '%s' doesn't seem to exist\n", path);
		return 1;
	}

	if (mkdir(".package", 0700) < 0 && errno != EEXIST) {
		fprintf(stderr, "[AQUA Manager] ERROR Failed to create package directory at '%s/.package/'\n", path);
		return 1;
	}

	if (unique) {
		write_file(".package/unique", unique);
	}

	if (mkdir(".build", 0700) < 0 && errno != EEXIST) {
		fprintf(stderr, "[AQUA Manager] ERROR Failed to create build directory at '%s/.build/'\n", path);
		return 1;
	}

	switch (type) {
		case TYPE_ZASM: {
			write_file("main.zasm",
				"%hello_world \"Hello world!\" xa 0%\n\n"
				":main:\n\n"
				"\tmov a0 hello_world\n"
				"\tcal g0 print"
			);

			write_file(".build/source", "main.zasm");
			write_file(".build/type", "zasm");

			break;
		}

		case TYPE_AMBER: {
			write_file("main.a",
				"kfunc print \"Hello world!\\n\";"
			);
			
			write_file(".build/source", "main.a");
			write_file(".build/type", "amber");

			break;
		}

		case TYPE_NATIVE_C: {
			write_file(".package/start", "native");
			
			write_file("main.c",
				"#include <stdio.h>\n\n"
				"int main(void) {\n"
				"\tprintf(\"Hello world!\\n\");\n"
				"\treturn 0;\n"
				"}"
			);
			
			write_file("build.sh",
				"#!/bin/sh\n"
				"set -e\n\n"
				"time cc main.c -I/usr/share/aqua/lib/c/ -shared -fPIC -o .package/native.bin\n"
				"time iar --pack .package/ --output package.zpk"
			);

			break;
		}
		
		case TYPE_NATIVE_CPP: {
			write_file(".package/start", "native");

			write_file("main.cpp",
				"#include <iostream>\n\n"
				"int main(void) {\n"
				"\tstd::cout << \"Hello world!\" << std::endl;\n"
				"\treturn 0;\n"
				"}"
			);
			
			write_file("build.sh",
				"#!/bin/sh\n"
				"set -e\n\n"
				"time c++ main.cpp -shared -fPIC -o .package/native.bin\n"
				"time iar --pack .package/ --output package.zpk"
			);
			
			break;
		}

		case TYPE_UNCHANGED:
		default: break;
	}

	if (mkdir(".package/meta", 0700) < 0 && errno != EEXIST) {
		fprintf(stderr, "[AQUA Manager] ERROR Failed to create meta directory at '%s/.package/meta/'\n", path);
		return 1;
	}

	if (name) write_file(".package/meta/name", name);
	if (description) write_file(".package/meta/description", description);
	if (version) write_file(".package/meta/version", version);
	if (author) write_file(".package/meta/author", author);
	if (organization) write_file(".package/meta/organization", organization);

	printf("[AQUA Manager] Done\n");
	return 0;
}

static int create(void) {
	printf("[AQUA Manager] Creating new project ...\n");

	printf("[AQUA Manager] Setting defaults ...\n");

	if (!type) {
		type = TYPE_NATIVE_C;
	}

	if (!name) name = DEFAULT_NAME;
	if (!description) description = DEFAULT_DESCRIPTION;
	if (!version) version = DEFAULT_VERSION;
	
	if (!author) {
		author = DEFAULT_AUTHOR;
		
		struct passwd* passwd = getpwuid(getuid());
		if (passwd) author = strdup(passwd->pw_name); // don't worry about freeing this either
	}

	if (!organization) {
		organization = DEFAULT_ORGANIZATION;

		int bytes = sysconf(_SC_HOST_NAME_MAX) + 1;

		organization = (char*) malloc(bytes); // don't worry about freeing this either either
		gethostname(organization, bytes);
	}

	if (!unique) {
		int64_t seconds = time(NULL);
		srand(seconds ^ hash(organization) ^ hash(author) ^ hash(name));

		unique = (char*) malloc(1024 /* this should be enough */); // don't worry about freeing this
		sprintf(unique, "%lx:%x", seconds, rand());
	}
	
	printf("[AQUA Manager] Creating project files ...\n");

	if (mkdir(path, 0700) < 0 && errno != EEXIST) {
		fprintf(stderr, "[AQUA Manager] ERROR Failed to create project directory at '%s'\n", path);
		return 1;
	}

	return modify();
}

int main(int argc, char** argv) {
	action_t action = ACTION_MODIFY;

	printf("[AQUA Manager] Parsing arguments ...\n");

	for (int i = 1; i < argc; i++) {
		if (strncmp(argv[i], "--", 2) == 0) {
			char* option = argv[i] + 2;

			if (strcmp(option, "create") == 0) {
				if (i != 1) {
					fprintf(stderr, "[AQUA Manager] ERROR '--create' option must be the first argument\n");
					return 1;
				}
				
				action = ACTION_CREATE;
			}

			else if (strcmp(option, "path") == 0) path = argv[++i];
			else if (strcmp(option, "unique") == 0) unique = argv[++i];

			else if (strcmp(option, "name") == 0) name = argv[++i];
			else if (strcmp(option, "description") == 0) description = argv[++i];
			else if (strcmp(option, "version") == 0) version = argv[++i];
			else if (strcmp(option, "author") == 0) author = argv[++i];
			else if (strcmp(option, "organization") == 0) organization = argv[++i];

			else if (strcmp(option, "type") == 0) {
				char* type_string = argv[++i];

				if (strcmp(type_string, "zasm") == 0) type = TYPE_ZASM;
				else if (strcmp(type_string, "amber") == 0) type = TYPE_AMBER;

				else if (strcmp(type_string, "native-c") == 0) type = TYPE_NATIVE_C;
				else if (strcmp(type_string, "native-c++") == 0) type = TYPE_NATIVE_CPP;

				else {
					fprintf(stderr, "[AQUA Manager] ERROR Type '%s' is unknown\n", type_string);
					return 1;
				}
			}

			else {
				fprintf(stderr, "[AQUA Manager] ERROR Option '--%s' is unknown\n", option);
				return 1;
			}

		} else {
			fprintf(stderr, "[AQUA Manager] ERROR Unexpected argument '%s'\n", argv[i]);
			return 1;
		}
	}

	if (action == ACTION_MODIFY) return modify();
	if (action == ACTION_CREATE) return create();
	
	return 1;
}