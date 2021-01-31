# Studio-Game-Model-Renderer-Hook

A full replacement for cs 1.6 R_StudioDrawModel & R_StudioDrawPlayer functions. Full source code for theese two functions is included, aswell as hooking mechanisms to hook theese two functions. The minimum to run this code is hook for cl_enginefunc_t & cl_clientfunc_t.

# Source code
The source code is written in like one hour, so be aware that it might have some bugs. It is just an demonstration of what you can actually do - replace whole model and player rendering functions with your own rendering algorithms. The code excludes part with cl_minmodels, because it requires a hook to get the teamnumber and vip flags, which as far as i know are only obtainable through usermsg. The code has been taken from the public client.dll source code.

# Proof
And theres a little proof, that this hook actually works.

![alt text](https://i.imgur.com/U0gNOqR.png)
