
#include "Session.h"

Socket* Session::get_socket()
{
    return this->_sock.get();
}

void Session::start_send()
{   
    this->file_desc.emplace(this->_sock->get_raw_socket().get_executor(), this->_transaction->fd);
    if(this->_transaction->error())
    {
        logger::debug("ERROR", "start_send", std::to_string(_transaction->status), __FILE__, __LINE__);
        return;
    }
    this->bytes_transferred = 0;
    this->send_resource(0);
}

void Session::send_resource(std::size_t bytes_read)
{
    auto self = shared_from_this();
    self->file_desc->async_read_some(asio::buffer(this->_buffer.data() + bytes_read, this->_buffer.size() - bytes_read),
    [self, bytes_read](const asio::error_code& error, std::size_t bytes) mutable
    {
        if(self->_transaction->error(error) && error != asio::error::eof)
        {
            logger::debug("ERROR", "start_send", std::to_string(self->_transaction->status), __FILE__, __LINE__);
            return;
        }

        self->bytes_transferred += bytes;
        bytes_read += bytes;

        self->_sock->do_write(self->_buffer.data() + bytes_read - bytes, bytes,
        [self, bytes_read](const asio::error_code& error, std::size_t bytes_written) mutable
        {
            if(self->bytes_transferred >= self->_transaction->content_len || error == asio::error::eof)
            {
                logger::debug("INFO", "SEND COMPLETE", std::to_string(self->bytes_transferred) + " bytes", __FILE__, __LINE__);
                logger::log("INFO", self);
                self->get_socket()->close();
                return;
            }
            if(self->_buffer.size() - bytes_read < 10000)
            {
                memset(self->_buffer.data(), '\0', self->_buffer.size());
                bytes_read = 0; // reset pointer
            }
            if(self->_transaction->error(error))
            {
                logger::debug("ERROR", "do_write", error.message(), __FILE__, __LINE__);
            }
            self->send_resource(bytes_read);
        }); 
    });
}

void Session::handle_request(const asio::error_code& error)
{
    auto self = shared_from_this();
    
    if(self->_transaction->error(error))
    {
        logger::debug("ERROR", "async_read_some", error.message() , __FILE__, __LINE__);
        logger::log("ERROR" + error.message());
        return;
    }

    self->_transaction->process(self->_buffer);
    self->_sock->do_write(self->_transaction->resp_header.data(), strlen(self->_transaction->resp_header.data()),
    [self](const asio::error_code& error, std::size_t bytes)
    {
        if(self->_transaction->error(error))
        {
            logger::debug("ERROR", "writing header", std::to_string(self->get_transaction()->status) , __FILE__, __LINE__);
            return;
        }
        logger::debug("REQUEST HEADER", "", std::string(self->_buffer.begin(), self->_buffer.end()), __FILE__, __LINE__);
        logger::debug("RESPONSE HEADER", "", std::string(self->_transaction->resp_header.begin(), self->_transaction->resp_header.end()), __FILE__, __LINE__);
        self->start_send();
    });
}

void Session::start()
{
    auto self = shared_from_this();

    self->_buffer.resize(BUFFER_SIZE);
    
    self->_sock->do_handshake (
    [self](const asio::error_code& error)
    {
        if(self->_transaction->error(error))
        {
            logger::debug("ERROR", "do_handshake", error.message(), __FILE__, __LINE__);
            return;
        }

        self->_sock->do_read(self->_buffer.data(), self->_buffer.size(), 
        [self](const asio::error_code& error, std::size_t bytes)
        {
            self->handle_request(error);
        });
    });
}

Transaction* Session::get_transaction()
{
    return _transaction.get();
}