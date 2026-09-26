#include <cstring>
#include <deque>
#include <list>
#include <memory>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <string>
#include <pmos/helpers.hh>
#include <charconv>
#include <pmos/async/coroutines.hh>
#include <iostream>
#include <map>
#include <pmos/ipc/bus_object.hh>
#include <cstddef>
#include <system_error>


pmos::Port main_port = pmos::Port::create().value();
pmos::PortDispatcher dispatcher(main_port);

const auto port_name = "/pmos/pmbus";

struct PublishedObject {
    pmos::ipc::BUSObject object;
    pmos::Right right;
    uint64_t sequence_number;
};
uint64_t next_sequence_number = 1;
std::map<uint64_t, std::shared_ptr<PublishedObject>> published_objects;

void handle_publish_reply(pmos::Right reply_right, int32_t result, uint64_t sequence_number)
{
    IPC_BUS_Publish_Object_Reply reply = {
        .type = IPC_BUS_Publish_Object_Reply_NUM,
        .flags = 0,
        .result = result,
        .reserved = 0,
        .sequence_number = sequence_number,
    };

    auto ptr = reinterpret_cast<const uint8_t *>(&reply);
    std::span<const uint8_t> span(ptr, sizeof(reply));

    auto result_send = pmos::send_message_right(reply_right, span, std::pair{&dispatcher.get_port(), pmos::RightType::SendOnce}, true);
    if (!result_send)
        std::cerr << "pmbus failed to send reply: " << result_send.error() << std::endl;
}

pmos::async::detached_task watch_right_deletion(pmos::ReceiveRight right, std::shared_ptr<PublishedObject> object)
{
    co_await dispatcher.get_message(right);

    published_objects.erase(object->sequence_number);
    object->right.release();
}

void handle_publish_object(std::span<const uint8_t> data, pmos::Right reply_right, pmos::Right object_right)
{
    auto *msg = reinterpret_cast<const IPC_BUS_Publish_Object *>(data.data());
    if (msg->type != IPC_BUS_Publish_Object_NUM) {
        std::cerr << "pmbus received message that is not IPC_BUS_Publish_Object" << std::endl;
        return;
    }

    pmos::ipc::BUSObject object;
    try {
        object = pmos::ipc::BUSObject::deserialize(data.subspan(offsetof(IPC_BUS_Publish_Object, object)));
    } catch (std::system_error &e) {
        std::cerr << "pmbus failed to deserialize object: " << e.what() << std::endl;
        handle_publish_reply(std::move(reply_right), e.code().value(), 0);
        return;
    }

    auto watch_result = pmos::watch_right(main_port, object_right);
    if (!watch_result) {
        handle_publish_reply(std::move(reply_right), watch_result.error(), 0);
        return;
    }

    auto sequence_number = next_sequence_number++;
    auto ptr = std::make_shared<PublishedObject>(std::move(object), std::move(object_right), sequence_number);
    published_objects[sequence_number] = ptr;

    watch_right_deletion(std::move(watch_result.value()), ptr);

    handle_publish_reply(std::move(reply_right), 0, sequence_number);

    // TODO...
}

pmos::async::detached_task handle_messages(pmos::ReceiveRight receive_right)
{
    std::cout << "pmbus getting messages" << std::endl;

    while (true) {
        auto msg = co_await dispatcher.get_message(receive_right);

        if (msg->descriptor.size < IPC_MIN_SIZE) {
            std::cerr << "pmbus received message that is too small" << std::endl;
            continue;
        }

        IPC_Generic_Msg *generic_msg = reinterpret_cast<IPC_Generic_Msg *>(msg->data.data());
        switch (generic_msg->type) {
        case IPC_BUS_Publish_Object_NUM:
            if (msg->descriptor.size < sizeof(IPC_BUS_Publish_Object)) {
                std::cerr << "pmbus received message that is too small for IPC_BUS_Publish_Object" << std::endl;
                break;
            }

            handle_publish_object(std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(msg->data.data()), msg->data.size()), std::move(msg->reply_right), std::move(msg->other_rights[0]));
            break;

        default:
            std::cout << "pmbus received unknown message type: " << generic_msg->type << std::endl;
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    auto [send_right, receive_right] = main_port.create_right(pmos::RightType::SendMany).value();
    pmos::name_right(std::move(send_right), port_name).value();
    handle_messages(std::move(receive_right));
    auto val = dispatcher.dispatch();

    std::cout << "pmbus dispatcher ended" << std::endl;

    if (!val)
        return val.error();
    return 0;
}