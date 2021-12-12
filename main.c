
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <errno.h>
#include <cjson/cJSON.h>

typedef enum {
	ACTION_CREATE, ACTION_LAYOUT
} action_t;

typedef enum {
	TYPE_UNCHANGED = 0,
	TYPE_CUSTOM,
	TYPE_ZASM, TYPE_AMBER,
	TYPE_NATIVE_C, TYPE_NATIVE_CPP,
	TYPE_LEN
} type_t;

#define DEFAULT_PATH "./"

static type_t type = TYPE_UNCHANGED;
static char* path = DEFAULT_PATH;

// properties necessary for the functioning of the app

#define DEFAULT_START "native" // for now...

static char* unique = NULL;
static char* start = NULL;
static char* entry = NULL;

// metadata properties

#define DEFAULT_NAME "Untitled Project"
#define DEFAULT_DESCRIPTION "Untitled project which has no title."
#define DEFAULT_VERSION "development"

#define DEFAULT_AUTHOR "Anonymousia de Bergerac-Fleur"
#define DEFAULT_ORGANIZATION "Literally the CIA"

static char* name = NULL;
static char* description = NULL;
static char* version = NULL;

static char* author = NULL;
static char* organization = NULL;

// truly optional metadata properties

static char* website = NULL;
static char* icon = NULL;

static char* read_file(const char* path) {
	FILE* fp = fopen(path, "rb");

	if (!fp) {
		return NULL; // return 'NULL' if file don't exist
	}

	fseek(fp, 0, SEEK_END);

	unsigned bytes = ftell(fp);
	char* buffer = calloc(1, bytes + 1); // create null-byte

	rewind(fp);
	
	if (fread(buffer, 1, bytes, fp) != bytes) {
		free(buffer);
		buffer = NULL; // return 'NULL' if something went wrong reading
	}

	fclose(fp);
	return buffer;
}

static void write_file(const char* path, const char* value) {
	if (!value) {
		return;
	}

	FILE* fp = fopen(path, "wb");
	
	if (!fp) {
		return; // if something goes wrong, too bad, just ignore it
	}

	fprintf(fp, "%s", value);
	fclose(fp);
}

static uint64_t hash(const char* string) { // djb2 algorithm
	uint64_t hash = 5381;
	
	char c;
	while ((c = *string++)) {
		hash = ((hash << 5) + hash) + c; // hash * 33 + c
	}

	return hash;
}

// some systems don't have a 'time' command, so we'll have to check for that if we don't want an error to be thrown

#define FIND_TIMEIT "\nif [ $(which time) ]; then\n" \
	"\tTIMEIT=time\n" \
	"fi\n\n"

static void template_nothing(void) {
	// do nothing lol
}

static void template_zasm(void) {
	write_file("main.zasm",
		"%hello_world \"Hello world!\" xa 0%\n\n"
		":main:\n\n"
		"\tmov a0 hello_world\n"
		"\tcal g0 print"
	);

	write_file(".build/source", "main.zasm");
	write_file(".build/type", "zasm");
}

static void template_amber(void) {
	write_file("main.a",
		"kfunc print \"Hello world!\\n\";"
	);
	
	write_file(".build/source", "main.a");
	write_file(".build/type", "amber");
}

static void template_native_c(void) {
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
		"set -e\n"
		FIND_TIMEIT
		"$TIMEIT cc main.c -I/usr/local/share/aqua/lib/c/ -shared -fPIC -o .package/entry.native\n"
		"$TIMEIT aqua-manager --layout\n"
		"$TIMEIT iar --pack .package/ --output package.zpk"
	);
}

static void template_native_cpp(void) {
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
		"set -e\n"
		FIND_TIMEIT
		"$TIMEIT c++ main.cpp -shared -fPIC -o .package/entry.native\n"
		"$TIMEIT aqua-manager --layout\n"
		"$TIMEIT iar --pack .package/ --output package.zpk"
	);
}

static char* gen_unique(void) {
	int64_t seconds = time(NULL);
	srand(seconds ^ hash(organization) ^ hash(author) ^ hash(name));

	char* buffer = malloc(1024 /* this should be enough */); // don't worry about freeing this
	sprintf(buffer, "%lx:%x", seconds, rand());

	return buffer;
}

static char* gen_author(void) {
	char* buffer = DEFAULT_AUTHOR;
	
	struct passwd* passwd = getpwuid(getuid());
	
	if (passwd) {
		buffer = strdup(passwd->pw_name); // don't worry about freeing this either
	}

	return buffer;
}

static char* gen_organization(void) {
	char* buffer = DEFAULT_ORGANIZATION;

	int bytes = sysconf(_SC_HOST_NAME_MAX) + 1;

	buffer = (char*) malloc(bytes); // don't worry about freeing this either either
	gethostname(buffer, bytes);

	return buffer;
}

static char* json_str(cJSON* json, const char* key) {
	const cJSON* item = cJSON_GetObjectItemCaseSensitive(json, key);

	if (!item) {
		return NULL;
	}

	if (!cJSON_IsString(item) || !item->valuestring) {
		fprintf(stderr, "[AQUA Manager] WARNING '%s' is not a string\n", key);
		return NULL;
	}

	return item->valuestring;
}

static void get_default_str(char** str_ref, const char* _default) {
	if (!*str_ref) {
		*str_ref = (char*) _default;
	}
}

static void get_default_func(char** str_ref, char* (*func) (void)) {
	if (!*str_ref) {
		*str_ref = func();
	}
}

static inline int layout(void) {
	printf("[AQUA Manager] Changing to project path '%s' ...\n", path);

	if (chdir(path) < 0) {
		fprintf(stderr, "[AQUA Manager] ERROR Project path '%s' doesn't seem to exist\n", path);
		return -1;
	}

	printf("[AQUA Manager] Parsing 'meta.json' if it exists ...\n");
	char* meta_raw = read_file("meta.json");

	if (!meta_raw) {
		fprintf(stderr, "[AQUA Manager] WARNING Could not find 'meta.json'; continuing with defaults ...\n");
		goto skip;
	}

	cJSON_Minify(meta_raw); // XXX to remove JSON comments
	cJSON* meta = cJSON_Parse(meta_raw);

	if (!meta) {
		fprintf(stderr, "[AQUA Manager] WARNING Could not parse 'meta.json'; continuing with defaults ...\n");
		goto skip;
	}

	// metadata properties

	name = json_str(meta, "name");
	description = json_str(meta, "description");
	version = json_str(meta, "version");
	
	author = json_str(meta, "author");
	organization = json_str(meta, "organization");

	// truly optional metadata properties

	website = json_str(meta, "website");
	icon = json_str(meta, "icon");

	// properties necessary for the functioning of the app

	unique = json_str(meta, "unique");
	start = json_str(meta, "start");
	entry = json_str(meta, "entry");

skip:

	// metadata properties

	get_default_str(&name, DEFAULT_NAME);
	get_default_str(&description, DEFAULT_DESCRIPTION);
	get_default_str(&version, DEFAULT_VERSION);
	
	get_default_func(&author, gen_author);
	get_default_func(&organization, gen_organization);

	// properties necessary for the functioning of the app

	get_default_func(&unique, gen_unique); // put this after everything incase we need to call 'gen_unique'
	get_default_str(&start, DEFAULT_START);

	if (strcmp(start, "zed") == 0) get_default_str(&entry, "entry.zed");
	else if (strcmp(start, "native") == 0) get_default_str(&entry, "entry.native");
	else if (strcmp(start, "system") == 0) get_default_str(&entry, "entry.sh");

	else {
		fprintf(stderr, "[AQUA Manager] ERROR Unknown start value '%s'\n", start);
		return -1;
	}

	// layout setup

	printf("[AQUA Manager] Laying project out ...\n");

	if (mkdir(".package", 0700) < 0 && errno != EEXIST) {
		fprintf(stderr, "[AQUA Manager] ERROR Failed to create package directory at '%s/.package/'\n", path);
		return -1;
	}

	// type-specific code

	if (mkdir(".build", 0700) < 0 && errno != EEXIST) {
		fprintf(stderr, "[AQUA Manager] ERROR Failed to create build directory at '%s/.build/'\n", path);
		return -1;
	}

	void (*TYPE_LUT[TYPE_LEN]) (void);

	for (int i = 0; i < sizeof(TYPE_LUT) / sizeof(*TYPE_LUT); i++) {
		TYPE_LUT[i] = template_nothing;
	}

	TYPE_LUT[TYPE_ZASM] = template_zasm;
	TYPE_LUT[TYPE_AMBER] = template_amber;
	TYPE_LUT[TYPE_NATIVE_C] = template_native_c;
	TYPE_LUT[TYPE_NATIVE_CPP] = template_native_cpp;

	TYPE_LUT[type]();

	// write properties

	if (chdir(".package") < 0) {
		fprintf(stderr, "[AQUA Manager] ERROR Failed to move into '.package' to write properties\n");
		return -1;
	}

	// properties necessary for the functioning of the app

	write_file("unique", unique);
	write_file("start", start);
	write_file("entry", entry);

	// metadata properties

	write_file("name", name);
	write_file("description", description);
	write_file("version", version);

	write_file("author", author);
	write_file("organization", organization);

	// truly optional metadata properties

	write_file("website", website);
	write_file("icon", icon);

	printf("[AQUA Manager] Done\n");
	return 0;
}

static int create(void) {
	printf("[AQUA Manager] Creating new project ...\n");

	if (!type) {
		type = TYPE_NATIVE_C;
	}

	printf("[AQUA Manager] Creating project files ...\n");

	if (mkdir(path, 0700) < 0 && errno != EEXIST) {
		fprintf(stderr, "[AQUA Manager] ERROR Failed to create project directory at '%s'\n", path);
		return -1;
	}

	return layout();
}

int main(int argc, char** argv) {
	action_t action = ACTION_LAYOUT;

	printf("[AQUA Manager] Parsing arguments ...\n");

	for (int i = 1; i < argc; i++) {
		if (strncmp(argv[i], "--", 2)) {
			fprintf(stderr, "[AQUA Manager] ERROR Unexpected argument '%s'\n", argv[i]);
			return -1;
		}

		char* option = argv[i] + 2;

		if (strcmp(option, "create") == 0) {
			if (i != 1) {
				fprintf(stderr, "[AQUA Manager] ERROR '--create' option must be the first argument\n");
				return -1;
			}
			
			action = ACTION_CREATE;
		}

		else if (strcmp(option, "layout") == 0) {
			if (i != 1) {
				fprintf(stderr, "[AQUA Manager] ERROR '--layout' option must be the first argument\n");
				return -1;
			}

			action = ACTION_LAYOUT;
		}

		else if (strcmp(option, "path") == 0) {
			path = argv[++i];
		}

		else if (strcmp(option, "type") == 0) {
			char* type_string = argv[++i];

			if (strcmp(type_string, "custom") == 0) type = TYPE_CUSTOM;

			else if (strcmp(type_string, "zasm") == 0) type = TYPE_ZASM;
			else if (strcmp(type_string, "amber") == 0) type = TYPE_AMBER;

			else if (strcmp(type_string, "native-c") == 0) type = TYPE_NATIVE_C;
			else if (strcmp(type_string, "native-c++") == 0) type = TYPE_NATIVE_CPP;

			else {
				fprintf(stderr, "[AQUA Manager] ERROR Type '%s' is unknown\n", type_string);
				return -1;
			}
		}

		else {
			fprintf(stderr, "[AQUA Manager] ERROR Option '--%s' is unknown\n", option);
			return -1;
		}
	}

	if (action == ACTION_LAYOUT) return layout();
	if (action == ACTION_CREATE) return create();
	
	return -1;
}
