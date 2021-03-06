#ifndef HEADER_REQUEST_CONNECTION_HPP
#define HEADER_REQUEST_CONNECTION_HPP

#include "network/protocol.hpp"
#include "online/xml_request.hpp"

class RequestConnection : public Protocol
{
protected:
    /** Id of the server to join. */
    uint32_t           m_server_id;

    /** The request to join a server. */
    Online::XMLRequest *m_request;
    enum STATE
    {
        NONE,
        REQUEST_PENDING,
        DONE,
        EXITING
    };

    /** State of this connection. */
    STATE m_state;

public:
    // --------------------------------------------------------------------
    /** A simple request class to ask to join a server.
     */
    class ServerJoinRequest : public Online::XMLRequest
    {
        virtual void callback();
    public:
        ServerJoinRequest() : Online::XMLRequest() {}
    };   // ServerJoinRequest
    // --------------------------------------------------------------------


             RequestConnection(uint32_t server_id);
    virtual ~RequestConnection();

    virtual bool notifyEvent(Event* event) { return true; }
    virtual bool notifyEventAsynchronous(Event* event) { return true; }
    virtual void setup();
    virtual void update() {}
    virtual void asynchronousUpdate();


};   // RequestConnection

#endif // REQUEST_CONNECTION_HPP
