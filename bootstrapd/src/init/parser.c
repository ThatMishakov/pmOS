#include "../io.h"
#include "init.h"

#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <yaml.h>

enum parser_states {
    STATE_START,
    STATE_STREAM,
    STATE_DOCUMENT,
    STATE_SECTION,
};
struct parser_state {
    enum parser_states state;
    struct Service *service;
};

// static bool parse_service(struct parser_state *state);
// {

// }

static bool skip_event(yaml_parser_t *state, yaml_event_t *event);

static bool skip_sequence(yaml_parser_t *state, yaml_event_t *event)
{
    (void)event;

    bool cont = true;

    do {
        bool success = true;

        yaml_event_t new_event;
        if (!yaml_parser_parse(state, &new_event)) {
            print_str("Failed to parse event!\n");
            return false;
        }

        switch (new_event.type) {
        case YAML_SEQUENCE_END_EVENT:
            cont = false;
            break;

        case YAML_SEQUENCE_START_EVENT:
        case YAML_MAPPING_START_EVENT:
        case YAML_ALIAS_EVENT:
        case YAML_SCALAR_EVENT:
            success = skip_event(state, &new_event);
            break;

        default:
            print_str("Unknown event in skip_sequence: ");
            print_hex(new_event.type);
            print_str("\n");
            success = false;
        }

        yaml_event_delete(&new_event);
        if (!success)
            return false;
    } while (cont);
    return true;
}

static bool skip_mapping(yaml_parser_t *state, yaml_event_t *event)
{
    (void)event;
    bool cont = true;

    do {
        bool success = true;

        yaml_event_t new_event;
        if (!yaml_parser_parse(state, &new_event)) {
            print_str("Failed to parse event!\n");
            return false;
        }

        switch (new_event.type) {
        case YAML_MAPPING_END_EVENT:
            cont = false;
            break;

        case YAML_SEQUENCE_START_EVENT:
        case YAML_MAPPING_START_EVENT:
        case YAML_ALIAS_EVENT:
        case YAML_SCALAR_EVENT:
            success = skip_event(state, &new_event);
            break;

        default:
            print_str("Unknown event in skip_mapping: ");
            print_hex(new_event.type);
            print_str("\n");
            success = false;
        }

        yaml_event_delete(&new_event);
        if (!success)
            return false;
    } while (cont);
    return true;
}

static bool skip_event(yaml_parser_t *state, yaml_event_t *event)
{
    bool success = true;

    switch (event->type) {
    case YAML_ALIAS_EVENT:
    case YAML_SCALAR_EVENT:
        break;
    case YAML_SEQUENCE_START_EVENT:
        success = skip_sequence(state, event);
        break;
    case YAML_MAPPING_START_EVENT:
        success = skip_mapping(state, event);
        break;
    default:
        print_str("Unknown event ");
        print_hex(event->type);
        print_str("\n");
        success = false;
        break;
    }

    return success;
}

enum ServiceParseState {
    SERVICE_STATE_START,
    SERVICE_STATE_NAME,
    SERVICE_STATE_DESCRIPTION,
    SERVICE_STATE_RUN_TYPE,
    SERVICE_STATE_REQUIRE,
    SERVICE_STATE_MATCH,
    SERVICE_STATE_SKIP,
    SERVICE_STATE_END,
};

const char *SERVICE_NAME        = "name";
const char *SERVICE_DESCRIPTION = "description";
const char *SERVICE_RUN_TYPE    = "run_type";
const char *SERVICE_REQUIRE     = "require";
const char *SERVICE_MATCH       = "match";

const char *FILTER_PCI_CLASS    = "class";
const char *FILTER_PCI_SUBCLASS = "subclass";
const char *FILTER_PCI_PROG_IF  = "interface";

static bool parse_pci(yaml_parser_t *state, struct Service *service)
{
    bool cont = true;
    struct MatchFilter f = {
        .key = strdup("pci"),
        .pci_filter = {},
    };
    if (!f.key) {
        print_str("Failed to allocate memory for match filter key\n");
        return false;
    }

    yaml_event_t filter_event;
    if (!yaml_parser_parse(state, &filter_event)) {
        print_str("Failed to parse event!\n");
        free(f.key);
        return false;
    }

    if (filter_event.type == YAML_SEQUENCE_END_EVENT) {
        print_str("Unexpected YAML_SEQUENCE_END_EVENT in match filter\n");
        cont = false;
        goto end;
    }
    if (filter_event.type != YAML_MAPPING_START_EVENT) {
        print_str("Expected YAML_MAPPING_START_EVENT for match filter values, got ");
        print_hex(filter_event.type);
        print_str("\n");
        cont = skip_event(state, &filter_event);
        goto end;
    }

    yaml_event_t mapping;
    if (!yaml_parser_parse(state, &mapping)) {
        print_str("Failed to parse event!\n");
        cont = false;
        goto end;
    }
    while (cont && mapping.type != YAML_MAPPING_END_EVENT) {
        if (mapping.type != YAML_SCALAR_EVENT) {
            print_str("Expected YAML_SCALAR_EVENT for match filter entry, got ");
            print_hex(mapping.type);
            print_str("\n");
            if (!skip_event(state, &mapping)) {
                cont = false;
                break;
            }
        }

        const char *value = (char *)mapping.data.scalar.value;
        char **filter_entry = NULL;
        if (!strcmp(value, FILTER_PCI_CLASS))
            filter_entry = &f.pci_filter.class;
        else if (!strcmp(value, FILTER_PCI_SUBCLASS))
            filter_entry = &f.pci_filter.subclass;
        else if (!strcmp(value, FILTER_PCI_PROG_IF))
            filter_entry = &f.pci_filter.prog_if;
        else {
            print_str("Warning: Unrecognized PCI filter parameter: ");
            print_str(value);
            print_str("\n");
        }

        if (filter_entry && *filter_entry) {
            print_str("Warning: Duplicate PCI filter parameter: ");
            print_str(value);
            print_str("\n");
        }

        yaml_event_delete(&mapping);
        if (!yaml_parser_parse(state, &mapping)) {
            print_str("Failed to parse event!\n");
            cont = false;
            goto end;
        }

        if (mapping.type == YAML_MAPPING_END_EVENT) {
            print_str("Unexpected YAML_MAPPING_END_EVENT when parsing PCI filter!\n");
            cont = false;
            break;
        } else if (mapping.type != YAML_SCALAR_EVENT) {
            print_str("Warning: Unexpected yaml event type when parsing PCI filter: ");
            print_hex(mapping.type);
            print_str("\n");
        } else {
            if (filter_entry && !*filter_entry) {
                *filter_entry = strdup((char *)mapping.data.scalar.value);
                if (!*filter_entry) {
                    print_str("Error: Failed to allocate memory for a filter\n");
                    cont = false;
                    break;
                }
            }
        }
        yaml_event_delete(&mapping);
        
        if (!yaml_parser_parse(state, &mapping)) {
            print_str("Failed to parse event!\n");
            cont = false;
            goto end;
        }
    }
    yaml_event_delete(&mapping);

    if (cont) {
        int res = 0;
        VECTOR_PUSH_BACK_CHECKED(service->match_filters, f, res);
        if (res != 0) {
            print_str("Failed to add match filter to the service!\n");
            cont = false;
            goto end;
        } else {
            memset(&f, 0, sizeof(f));
        }
    }

end:
    free(f.key);
    free(f.pci_filter.class);
    free(f.pci_filter.prog_if);
    free(f.pci_filter.subclass);
    yaml_event_delete(&filter_event); 
    return cont;
}

static bool parse_pnp(yaml_parser_t *state, struct Service *service)
{
    bool cont = true;

    yaml_event_t filter_event;
    if (!yaml_parser_parse(state, &filter_event)) {
        print_str("Failed to parse event!\n");
        return false;
    }

    if (filter_event.type == YAML_SEQUENCE_END_EVENT) {
        print_str("Unexpected YAML_SEQUENCE_END_EVENT in match filter\n");
        yaml_event_delete(&filter_event);
        cont = false;
    } else if (filter_event.type != YAML_SEQUENCE_START_EVENT) {
        print_str("Expected YAML_SEQUENCE_START_EVENT for match filter values, got ");
        print_hex(filter_event.type);
        print_str("\n");
        bool success = skip_event(state, &filter_event);
        yaml_event_delete(&filter_event);
        if (!success) {
            return false;
        }
    } else {
        struct MatchFilter f = {
            .key = strdup("pnp"),
        };
        if (!f.key) {
            print_str("Failed to allocate memory for match filter key!\n");
            yaml_event_delete(&filter_event);
            return false;
        }

        char **strings       = NULL;
        size_t strings_count = 0;

        bool cont2 = true;
        while (cont2) {
            yaml_event_t value_event;
            if (!yaml_parser_parse(state, &value_event)) {
                print_str("Failed to parse event!\n");
                free(f.key);
                free(strings);
                yaml_event_delete(&filter_event);
                return false;
            }

            if (value_event.type == YAML_SEQUENCE_END_EVENT) {
                cont2 = false;
            } else if (value_event.type != YAML_SCALAR_EVENT) {
                print_str("Expected YAML_SCALAR_EVENT for match filter value, got ");
                print_hex(value_event.type);
                print_str("\n");
                bool success = skip_event(state, &value_event);
                yaml_event_delete(&value_event);
                if (!success) {
                    free(f.key);
                    free(strings);
                    yaml_event_delete(&filter_event);
                    return false;
                }
            } else {
                char **new_strings = realloc(strings, sizeof(char *) * (strings_count + 2));
                if (!new_strings) {
                    print_str("Failed to allocate memory for match filter strings!\n");
                    free(f.key);
                    free(strings);
                    yaml_event_delete(&value_event);
                    yaml_event_delete(&filter_event);
                    return false;
                }
                strings                = new_strings;
                strings[strings_count] = strdup((char *)value_event.data.scalar.value);
                if (!strings[strings_count]) {
                    print_str("Failed to allocate memory for match filter string!\n");
                    free(f.key);
                    for (size_t i = 0; i < strings_count; i++)
                        free(strings[i]);
                    free(strings);
                    yaml_event_delete(&value_event);
                    yaml_event_delete(&filter_event);
                    return false;
                }
                strings_count++;
                strings[strings_count] = NULL;
                yaml_event_delete(&value_event);
            }
        }

        f.strings = strings;
        int res   = 0;
        VECTOR_PUSH_BACK_CHECKED(service->match_filters, f, res);
        if (res != 0) {
            print_str("Failed to add match filter to service!\n");
            free(f.key);
            for (size_t i = 0; i < strings_count; i++)
                free(strings[i]);
            free(strings);
            yaml_event_delete(&filter_event);
            return false;
        }
        yaml_event_delete(&filter_event);
    }

    return cont;
}

static bool parse_match_filter(yaml_parser_t *state, yaml_event_t *event, struct Service *service)
{
    if (event->type != YAML_MAPPING_START_EVENT) {
        print_str("Expected YAML_MAPPING_START_EVENT for match filter, got ");
        print_hex(event->type);
        print_str("\n");
        return skip_event(state, event);
    }

    bool cont = true;
    while (cont) {
        yaml_event_t new_event;
        if (!yaml_parser_parse(state, &new_event)) {
            print_str("Failed to parse event!\n");
            return false;
        }

        if (new_event.type == YAML_MAPPING_END_EVENT) {
            cont = false;
        } else if (new_event.type != YAML_SCALAR_EVENT) {
            print_str("Expected YAML_SCALAR_EVENT for match filter entry, got ");
            print_hex(new_event.type);
            print_str("\n");
            if (!skip_event(state, &new_event)) {
                yaml_event_delete(&new_event);
                return false;
            }
        } else {
            if (!strcmp((char *)new_event.data.scalar.value, "pnp")) {
                cont = parse_pnp(state, service);
            } else if (!strcmp((char *)new_event.data.scalar.value, "pci")) {
                cont = parse_pci(state, service);
            } else {
                print_str("Unknown match filter key: ");
                print_str((char *)new_event.data.scalar.value);
                print_str("\n");
            }
        }
        yaml_event_delete(&new_event);
    }

    return true;
}

static bool parse_service_event(yaml_parser_t *state, yaml_event_t *, struct parser_state *s)
{
    yaml_event_t event;
    struct Service *service              = NULL;
    bool success                         = true;
    bool has_run_type                    = false;
    enum ServiceParseState service_state = SERVICE_STATE_START;

    if (!yaml_parser_parse(state, &event)) {
        print_str("Failed to parse event!\n");
        return false;
    }

    if (event.type == YAML_MAPPING_END_EVENT) {
        print_str("Unexpected YAML_MAPPING_END_EVENT\n");
        yaml_event_delete(&event);
        return false;
    }

    if (event.type != YAML_MAPPING_START_EVENT) {
        print_str("Expected YAML_MAPPING_START_EVENT when parsing service, got ");
        print_hex(event.type);
        print_str("\n");
        success = skip_event(state, &event);
        goto end;
    }

    if (s->service) {
        print_str("Duplicate service definition! Skipping...\n");
        success = skip_event(state, &event);
        goto end;
    }

    service = new_service();
    if (!service) {
        print_str("Failed to allocate memory for service!\n");
        success = false;
        goto end;
    }

    while (service_state != SERVICE_STATE_END) {
        switch (service_state) {
        case SERVICE_STATE_START: {
            yaml_event_t new_event;
            if (!yaml_parser_parse(state, &new_event)) {
                print_str("Failed to parse event!\n");
                success = false;
                goto end;
            }

            if (new_event.type == YAML_MAPPING_END_EVENT) {
                s->service    = service;
                service       = NULL;
                service_state = SERVICE_STATE_END;
            } else if (new_event.type != YAML_SCALAR_EVENT) {
                print_str("Expected YAML_SCALAR_EVENT for service name, got ");
                print_hex(new_event.type);
                print_str("\n");
                success       = skip_event(state, &new_event);
                service_state = SERVICE_STATE_SKIP;
            } else {
                const char *value = (const char *)new_event.data.scalar.value;

                if (!strcmp(value, SERVICE_NAME)) {
                    service_state = SERVICE_STATE_NAME;
                } else if (!strcmp(value, SERVICE_DESCRIPTION)) {
                    service_state = SERVICE_STATE_DESCRIPTION;
                } else if (!strcmp(value, SERVICE_RUN_TYPE)) {
                    service_state = SERVICE_STATE_RUN_TYPE;
                } else if (!strcmp(value, SERVICE_REQUIRE)) {
                    service_state = SERVICE_STATE_REQUIRE;
                } else if (!strcmp(value, SERVICE_MATCH)) {
                    service_state = SERVICE_STATE_MATCH;
                } else {
                    print_str("Unknown service key: ");
                    print_str(value);
                    print_str("\n");
                    service_state = SERVICE_STATE_SKIP;
                }
            }

            yaml_event_delete(&new_event);
            break;
        }
        case SERVICE_STATE_NAME:
        case SERVICE_STATE_RUN_TYPE:
        case SERVICE_STATE_DESCRIPTION: {
            yaml_event_t new_event;
            if (!yaml_parser_parse(state, &new_event)) {
                print_str("Failed to parse event!\n");
                success = false;
                goto end;
            }

            if (new_event.type == YAML_MAPPING_END_EVENT) {
                print_str("Unexpected YAML_MAPPING_END_EVENT while parsing service name\n");
                yaml_event_delete(&new_event);
                success = false;
                goto end;
            } else if (new_event.type != YAML_SCALAR_EVENT) {
                print_str("Expected YAML_SCALAR_EVENT for service name, got ");
                print_hex(new_event.type);
                print_str("\n");
                success = skip_event(state, &new_event);
            } else {
                if (service_state == SERVICE_STATE_NAME) {
                    if (service->name) {
                        print_str("Duplicate service name! Skipping...\n");
                    } else {
                        service->name = strdup((char *)new_event.data.scalar.value);
                        if (!service->name) {
                            print_str("Failed to allocate memory for service name!\n");
                            success = false;
                        }
                    }
                } else if (service_state == SERVICE_STATE_DESCRIPTION) {
                    if (service->description) {
                        print_str("Duplicate service description! Skipping...\n");
                    } else {
                        service->description = strdup((char *)new_event.data.scalar.value);
                        if (!service->description) {
                            print_str("Failed to allocate memory for service description!\n");
                            success = false;
                        }
                    }
                } else if (service_state == SERVICE_STATE_RUN_TYPE) {
                    if (has_run_type) {
                        print_str("Duplicate service run type! Skipping...\n");
                    } else {
                        auto run_type = parse_run_type((char *)new_event.data.scalar.value);
                        if (run_type == RUN_UNKNOWN) {
                            print_str("Unknown run type: ");
                            print_str((char *)new_event.data.scalar.value);
                            print_str("\n");
                        } else {
                            service->run_type = run_type;
                            has_run_type      = true;
                        }
                    }
                }
            }

            yaml_event_delete(&new_event);
            if (success)
                service_state = SERVICE_STATE_START;
            else
                service_state = SERVICE_STATE_END;
            break;
        }
        case SERVICE_STATE_REQUIRE: {
            yaml_event_t new_event;
            if (!yaml_parser_parse(state, &new_event)) {
                print_str("Failed to parse event!\n");
                success = false;
                goto end;
            }

            if (new_event.type == YAML_MAPPING_END_EVENT) {
                service_state = SERVICE_STATE_START;
            } else if (new_event.type == YAML_SEQUENCE_START_EVENT) {
                yaml_event_t seq_event;
                while (true) {
                    if (!yaml_parser_parse(state, &seq_event)) {
                        print_str("Failed to parse event!\n");
                        success = false;
                        break;
                    }

                    if (seq_event.type == YAML_SEQUENCE_END_EVENT) {
                        yaml_event_delete(&seq_event);
                        break;
                    }

                    if (seq_event.type != YAML_SCALAR_EVENT) {
                        print_str("Expected YAML_SCALAR_EVENT in require sequence, got ");
                        print_hex(seq_event.type);
                        print_str("\n");
                        success = skip_event(state, &seq_event);
                    } else {
                        char *required_service_name = (char *)seq_event.data.scalar.value;

                        struct Requirement r = {};
                        r.service_name       = strdup(required_service_name);
                        if (!r.service_name) {
                            print_str("Failed to allocate memory for requirement service name!\n");
                            success = false;
                            yaml_event_delete(&seq_event);
                            break;
                        }
                        int result = 0;
                        VECTOR_PUSH_BACK_CHECKED(service->requirements, r, result);
                        if (result != 0) {
                            print_str("Failed to allocate memory for requirement!\n");
                            free(r.service_name);
                            success = false;
                            yaml_event_delete(&seq_event);
                            break;
                        }
                    }

                    yaml_event_delete(&seq_event);
                    if (success)
                        service_state = SERVICE_STATE_START;
                    else
                        service_state = SERVICE_STATE_END;
                }
            }
            break;
        }
        case SERVICE_STATE_MATCH: {
            yaml_event_t new_event;
            if (!yaml_parser_parse(state, &new_event)) {
                print_str("Failed to parse event!\n");
                success = false;
                goto end;
            }

            if (new_event.type == YAML_MAPPING_END_EVENT) {
                service_state = SERVICE_STATE_START;
            } else {
                bool result = parse_match_filter(state, &new_event, service);
                if (!result) {
                    print_str("Failed to parse match filter!\n");
                    success = false;
                }
            }

            yaml_event_delete(&new_event);
            if (success)
                service_state = SERVICE_STATE_START;
            else
                service_state = SERVICE_STATE_END;
            break;
        }
        case SERVICE_STATE_SKIP: {
            yaml_event_t new_event;
            if (!yaml_parser_parse(state, &new_event)) {
                print_str("Failed to parse event!\n");
                success = false;
                goto end;
            }

            if (new_event.type == YAML_MAPPING_END_EVENT) {
                service_state = SERVICE_STATE_END;
            } else {
                success = skip_event(state, &new_event);
            }

            yaml_event_delete(&new_event);
            break;
        }
        case SERVICE_STATE_END:
            __builtin_unreachable();
            break;
        }
    }

end:
    if (!success)
        free_service(service);
    yaml_event_delete(&event);
    return success;
}

bool skip_next_event(yaml_parser_t *state)
{
    yaml_event_t event;
    if (!yaml_parser_parse(state, &event)) {
        print_str("Failed to parse event!\n");
        return false;
    }

    bool success = skip_event(state, &event);
    yaml_event_delete(&event);
    return success;
}

static bool parse_event(yaml_parser_t *state, yaml_event_t *event, struct parser_state *s)
{
    switch (s->state) {
    case STATE_START:
        switch (event->type) {
        case YAML_STREAM_START_EVENT:
            s->state = STATE_STREAM;
            break;
        default:
            print_str("Unknown event ");
            print_hex(event->type);
            print_str("\n");
            break;
        }
        break;
    case STATE_STREAM:
        switch (event->type) {
        case YAML_DOCUMENT_START_EVENT:
            s->state = STATE_DOCUMENT;
            break;
        case YAML_STREAM_END_EVENT:
            return false;
            break;
        default:
            print_str("Unknown event ");
            print_hex(event->type);
            print_str(" in STATE_STREAM\n");
            break;
        }
        break;
    case STATE_DOCUMENT:
        switch (event->type) {
        case YAML_MAPPING_START_EVENT:
            s->state = STATE_SECTION;
            break;
        case YAML_SCALAR_EVENT:
            print_str("Unexpected scalar ");
            print_str((char *)event->data.scalar.value);
            print_str(". Skipping...\n");
            break;
        case YAML_DOCUMENT_END_EVENT:
            // End of the document. Only parse one...
            return false;
        default:
            print_str("Unknown event ");
            print_hex(event->type);
            print_str(" in STATE_DOCUMENT\n");
            return false;
        }
        break;
    case STATE_SECTION: {
        char *value = NULL;
        switch (event->type) {
        case YAML_SEQUENCE_START_EVENT:
            print_str("Unexpected sequence as key... Skipping\n");

            if (!skip_sequence(state, event))
                return false;

            goto skip_event;
        case YAML_MAPPING_START_EVENT:
            print_str("Unexpected mapping as key... Skipping\n");

            if (!skip_mapping(state, event))
                return false;

            goto skip_event;
        case YAML_ALIAS_EVENT:
            print_str("Aliases not supported! Skipping mapping\n");

            goto skip_event;
        case YAML_SCALAR_EVENT:
            value = (char *)event->data.scalar.value;

            if (!strcmp(value, "service")) {
                bool b = parse_service_event(state, event, s);
                if (!b)
                    return false;
                break;
            } else {
                print_str("Unknown key: ");
                print_str(value);
                print_str("\n");
            }

        skip_event: {
            yaml_event_t event;
            if (!yaml_parser_parse(state, &event)) {
                print_str("Failed to parse event!\n");
                return false;
            }

            if (event.type == YAML_MAPPING_END_EVENT) {
                print_str("Unexpected YAML_MAPPING_END_EVENT\n");
                yaml_event_delete(&event);
                return false;
            }

            bool b = skip_event(state, &event);

            yaml_event_delete(&event);
            if (!b)
                return false;
        }

        break;
        case YAML_MAPPING_END_EVENT:
            s->state = STATE_DOCUMENT;
            break;
        default:
            print_str("Unknown event ");
            print_hex(event->type);
            print_str(" in STATE_SECTION\n");
            return false;
        }
    }
    }

    return true;
}

void parse_service(const char *cmdline, const char *name, struct Service **out_service)
{
    print_str("Parsing module ");
    if (!name)
        print_str("<unknown>");
    else
        print_str(name);
    print_str("\n");

    struct parser_state state = {};

    yaml_parser_t parser;

    int done = 0;

    yaml_parser_initialize(&parser);

    size_t length = strlen(cmdline);

    yaml_parser_set_input_string(&parser, (const unsigned char *)cmdline, length);

    while (!done) {
        yaml_event_t event;
        if (!yaml_parser_parse(&parser, &event))
            goto error;

        bool t = parse_event(&parser, &event, &state);

        done = (event.type == YAML_STREAM_END_EVENT) || !t;
        yaml_event_delete(&event);
    }

    *out_service  = state.service;
    state.service = NULL;

    yaml_parser_delete(&parser);
    return;
error:
    free_service(state.service);
    *out_service  = NULL;
    state.service = NULL;

    yaml_parser_delete(&parser);
    print_str("Yaml parsing error\n");
    return;
}

enum RunType parse_run_type(const char *str)
{
    if (!strcasecmp(str, "manual"))
        return RUN_MANUAL;
    if (!strcasecmp(str, "always_once"))
        return RUN_ALWAYS_ONCE;
    if (!strcasecmp(str, "first_match_once"))
        return RUN_FIRST_MATCH_ONCE;
    if (!strcasecmp(str, "for_each_match"))
        return RUN_FOR_EACH_MATCH;
    return RUN_UNKNOWN;
}