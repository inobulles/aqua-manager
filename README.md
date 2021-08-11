# aqua-manager

This repo contains the source for the AQUA project manager (command-line), which is a tool for easily creating and managing AQUA projects.

## General usage

`aqua-manager` runs in two different modes; creation mode, which is used to create new projects, and modification mode, which is used to modify properties of preexisting projects. In creation mode, properties which are not explicitly given by the user will always be set to sensible default values, whereas in modification mode, properties will always be unchanged unless explicitly given by the user. `aqua-manager` runs in modification mode by default.

## Command-line arguments

Here is a list of all command-line arguments that can be passed to `aqua-manager` and how to use them:

### --create

Switch to creation mode. This will create the folder structure and files of the whole project.

### --path

The path in which to create a project when in creation mode or the path to the project in which to modify properties when in modification mode. The default value is set in `DEFAULT_PATH` and is `./`.

### --unique

Set the `unique` value of the project. This is meant to be a string which uniquely identifies any package. The default value is set following this algorithm:

```c
uint64_t hash(const char* string) { // djb2 algorithm
	uint64_t hash = 5381;
	
	char c;
	while ((c = *string++)) {
		hash = ((hash << 5) + hash) + c; // hash * 33 + c
	}

	return hash;
}

int64_t seconds = time(NULL);
srand(seconds ^ hash(organization_name) ^ hash(author_name) ^ hash(application_name));

char unique[1024];
sprintf(unique, "%lx:%x", seconds, rand());
```

where `srand` and `rand` are implementation-defined.

### --type

Set the project type. This will affect the folder structure. Valid project types are `zasm`, `amber`, `native-c`, and `native-c++`. As of aquaBSD 1.0 Alps, `native-c` is the default, but it will end up being `amber` in the future.

### --name

Set the name of the project. The default value is set in `DEFAULT_NAME` ("Untitled Project").

### --description

Set the description of the project. The default value is set in `DEFAULT_DESCRIPTION` ("Untitled project which has no title.").

### --version

Set the version of the project. The default value is set in `DEFAULT_VERSION` ("development").

### --author

Set the author name of the project. The default value will be the username of the current UID, or the value set in `DEFAULT_AUTHOR` ("Anonymousia de Bergerac-Fleur") if getting the username fails.

### --organization

Set the organization name of the project. The default value will be the hostname of the current machine, or the value set in `DEFAULT_ORGANIZATION` ("Literally the CIA") if getting the hostname fails.

## Installation

`aqua-manager` is automatically built and installed by the `aqua-unix` tool if you tell it to by passing the `--manager` argument like so:

```shell
% aqua-unix --manager --install
```