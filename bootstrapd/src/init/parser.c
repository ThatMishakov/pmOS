#include "../io.h"

#include <stdbool.h>
#include <string.h>
#include <yaml.h>
#include "init.h"
#include <strings.h>

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
    SERVICE_STATE_SKIP,
    SERVICE_STATE_END,
};

const char *SERVICE_NAME = "name";
const char *SERVICE_DESCRIPTION = "description";
const char *SERVICE_RUN_TYPE = "run_type";

static bool parse_service_event(yaml_parser_t *state, yaml_event_t *, struct parser_state *s)
{
    yaml_event_t event;
    struct Service *service = NULL;
    bool success = true;
    bool has_run_type = false;
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
                s->service = service;
                service = NULL;
                service_state = SERVICE_STATE_END;
            } else if (new_event.type != YAML_SCALAR_EVENT) {
                print_str("Expected YAML_SCALAR_EVENT for service name, got ");
                print_hex(new_event.type);
                print_str("\n");
                success = skip_event(state, &new_event);
                service_state = SERVICE_STATE_SKIP;
            } else {
                const char *value = (const char *)new_event.data.scalar.value;

                if (!strcmp(value, SERVICE_NAME)) {
                    service_state = SERVICE_STATE_NAME;
                } else if (!strcmp(value, SERVICE_DESCRIPTION)) {
                    service_state = SERVICE_STATE_DESCRIPTION;
                } else if (!strcmp(value, SERVICE_RUN_TYPE)) {
                    service_state = SERVICE_STATE_RUN_TYPE;
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
                            has_run_type = true;
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
        
        skip_event:
            {
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

    *out_service = state.service;
    state.service = NULL;

    yaml_parser_delete(&parser);
    return;
error:
    free_service(state.service);
    *out_service = NULL;
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