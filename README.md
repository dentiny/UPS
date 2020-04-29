### Final Project Backend
#### Introduction
In this project, backend system is mainly responsible for communicating with Amazon end, world and front end.
For Amazon and the world, since they may in the different server, it's natural to communicate via socket and protobuf. We designed our own protocal to ensure successful pick up, load and delivery.
While for front end, we have a wider range of IPC to select from. Like via socket, just like what we've done with the world; and also shared file, typically database. For the sake of convenience for Django to create database by models and the natural synchronzation of database itself, we choose postgres as the media of common access.

#### Detailed implementation
Since multiple communication with more-than-one subjects is required naturally, threading becomes a necessicity. Between the typical pre-thread via threadpool and per-threading by detaching, the latter seems to be a better solution here. 
Considering the fact that threads have to keep receiving all the time, it unwise to keep it in the threadpool, for it will occupy the pool capacity from start to end.
While for each request/response received, theoretically it's more reasonable to have them in threadpool, since that way, overhead of threading on critical path can be avoided, meanwhile hardware concurrency resource may not be exhausted. But considering the fact that it's hard for a single threadpool to have multiple executors in the same program, detaching also stands out here.

For  front-back end interaction, as I've said before, common database access is a viable and elegant way to implement. For front-end implemented in Django, ORM is taken under the hood; while for backend, C++ version postgres SQL manipulation is a prerequisite.

#### Other
- For debugging, thread-safe logger is implemented in singleton pattern.
- Following the Single Responsible Principle, classes in the backend are well designed to promise only one functionality, so it won't be hard for developers and code reviewers to read and understand.
- Though not required in the list, we also add creation time, pick up time, load time and delivery time to better help users to understand and track the package.

#### Amazon side link
[Amazon side link](https://github.com/yueyingyang/shopping-delivery-system)

#### World simulator
[World Simulator](https://github.com/yunjingliu96/world_simulator_exec)
