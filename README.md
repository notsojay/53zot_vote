# 53zot_vote

* Zot Vote is a secure, transparent, and anonymous voting system that leverages the power of concurrent server-client interaction to provide a seamless voting experience. It ensures that all votes are accurately accounted for and that the voting statistics are up-to-date.

## Features

* Anonymous Voting: Zot Vote respects user privacy. Users can cast their votes anonymously and check the ongoing poll results.
* Concurrent Server-Client Interaction: The server is capable of handling multiple client connections concurrently, ensuring smooth user experience during peak voting times.
* PetrV Protocol Support: The client and server communication is facilitated by the simple yet efficient PetrV Protocol.

## How It Works

* Upon startup, the Zot Vote server loads the current state of all available polls from a file. Once initialized, the server accepts client connections, spawning threads to handle all client communications until the client disconnects. This concurrent access to shared data structures on the server allows maintaining up-to-date voting information for each poll.
* Users can log into the Zot Vote server to view all the currently running polls and vote once for each poll anonymously. After casting their vote, users can request to see the current voting statistics for that poll until they log out.
* The base code includes an implementation of a client that follows the PetrV Protocol, ensuring the correct protocol implementation in your server program.

## Contributing

* Contributions to Zot Vote are welcome!

## Useful Resources

https://beej.us/guide/bgnet/ <br>
https://sourceware.org/gdb/onlinedocs/gdb/Threads.html <br>
https://beej.us/guide/bgnet/ <br>
https://www.wireshark.org/ <br>
https://www.tcpdump.org/manpages/tcpdump.1.html <br>
