# Telnet Server

This project is a simple implementation of a Telnet server. Telnet is a protocol used on the Internet or local area networks to provide a bidirectional interactive text-oriented communication facility using a virtual terminal connection.

## Features

- Supports multiple client connections
- Simple command interpretation
- Lightweight and fast


## Configuration

To start the server, configure:
```C++
telnet_server_config_t telnet_server_config = TELNET_SERVER_DEFAULT_CONFIG;
telnet_server_create(&telnet_server_config);
```

To change the listening port:
```C++
telnet_server_config_t telnet_server_config = TELNET_SERVER_DEFAULT_CONFIG;
telnet_server_config.port = 1023;
telnet_server_create(&telnet_server_config);
```

## Contributing

Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.

## License

[MIT](https://choosealicense.com/licenses/mit/)
