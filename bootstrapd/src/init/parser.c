#include <yaml.h>
#include "../io.h"
#include <stdbool.h>

enum parser_states {
    STATE_START,
    STATE_STREAM,
    STATE_DOCUMENT,
    STATE_SECTION,
};
struct parser_state {
    enum parser_states state;
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
    } while (cont) ;
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
    } while (cont) ;
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

            goto skip_node;
        case YAML_MAPPING_START_EVENT:
            print_str("Unexpected mapping as key... Skipping\n");

            if (!skip_mapping(state, event))
                return false;
            
            goto skip_node;
        case YAML_ALIAS_EVENT:
            print_str("Aliases not supported! Skipping mapping\n");

            goto skip_node;
        case YAML_SCALAR_EVENT:
            value = (char *)event->data.scalar.value;

        skip_node:
            //
            {
                print_str("Unknown key: ");
                print_str(value);
                print_str("\n");
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

void parse_service(const char *cmdline, const char *name)
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

    yaml_parser_delete(&parser);
    return;
error:
    yaml_parser_delete(&parser);
    print_str("Yaml parsing error\n");
    return;
}