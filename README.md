<!-- PROJECT LOGO -->
<br />
<p align="center">
 <a href="https://github.com/sassansh/SMTP-POP3-Servers">
    <img src="/images/logo.png" alt="Logo" width="80" height="80">
  </a>
  <h2 align="center">SMTP & POP3 Servers</h2>

  <p align="center">
     Implementation of two mail exchange servers: SMTP, for sending emails, and POP3, to retrieve emails. Built as a group programming assignment for UBC <a href="https://courses.students.ubc.ca/cs/courseschedule?pname=subjarea&tname=subj-course&dept=CPSC&course=317">CPSC 317</a> (Internet Computing).
  </p>
</p>

<!-- ![Assignment Question](/images/interface.png) -->

## Table of Contents

- [Goals 🎯](#goals-)
- [Technology Stack 🛠️](#technology-stack-)
- [Prerequisites 🍪](#prerequisites-)
- [Setup 🔧](#setup-)
- [Assignment Description 📚](#assignment-description-)
- [Team ‎😃](#team-)

## Goals 🎯

The goals of this assignment are:

- To learn how to program with TCP sockets in C;
- To learn how to read and implement a well-specified protocol;
- To develop your programming and debugging skills as they relate to the use of sockets in C;
- To implement the server side of a protocol;
- To develop general networking experimental skills;
- To develop a further understanding of what TCP does, and how to manage TCP connections from the server perspective.

## Technology Stack 🛠️

[C](https://www.cprogramming.com)

## Prerequisites 🍪

You should have [CLion](https://www.jetbrains.com/clion/) and [Git](https://git-scm.com/) installed on your PC.

## Setup 🔧

1. Clone the repo using:

   ```bash
     git clone https://github.com/sassansh/SMTP-POP3-Servers.git
   ```

2. Open the project in CLion.

## Assignment Description 📚

In this assignment you will use the Unix Socket API to construct two servers typically used in mail exchange: an SMTP server, used for sending emails, and a POP3 server, used to retrieve emails from a mailbox. The executables for these servers will be called, respectively, `mysmtpd` and `mypopd`. Both your programs are to take a single argument, the TCP port the respective server is to listen on for client connections.

To start your assignment, download the file [pa_email.zip](https://ca.prairielearn.com/pl/course_instance/2347/instance_question/12641537/clientFilesQuestion/pa_email.zip). This file contains the base files required to implement your code, as well as helper functions and a Makefile.

In order to avoid problems with spam and misbehaved email messages, your servers will implement only internal email messages. Messages will be received by your SMTP server, saved locally, and then retrieved by your POP3 server. Your messages will not be relayed to other servers, and you will not receive messages from other servers. You are not allowed to implement any email forwarding mechanism, as unintended errors when handling this kind of traffic may cause the department's network to be flagged as an email spammer, and you may be subject to penalties such as account suspension or limited access to the department's resources.

Some code is already provided that checks for the command-line arguments, listens for a connection, and accepts a new connection. Note that the provided code already implements functionality to *fork* each accepted client to a separate process, which means your system will naturally be able to handle multiple clients simultaneously without interfering with each other. Additionally, some provided functions also handle specific functionality needed for this assignment, including the ability to read individual lines from a socket and to handle usernames, passwords and email files.

## Team ‎😃

Sassan Shokoohi - [GitHub](https://github.com/sassansh) - [LinkedIn](https://www.linkedin.com/in/sassanshokoohi/) - [Personal Website](https://sassanshokoohi.ca)

Lana Kashino - [GitHub](https://github.com/lanakashino) - [LinkedIn](https://www.linkedin.com/in/lanakashino/) - [Personal Website](https://lanakashino.com)

Project Link: [https://github.com/sassansh/SMTP-POP3-Servers](https://github.com/sassansh/SMTP-POP3-Servers)

[linkedin-shield]: https://img.shields.io/badge/-LinkedIn-black.svg?style=for-the-badge&logo=linkedin&colorB=555
[linkedin-url]: https://www.linkedin.com/in/sassanshokoohi/
