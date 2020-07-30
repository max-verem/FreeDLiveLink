# FreeDLiveLink
LiveLink plugin for Unreal Engine (UE) to handle Free-D protol for camera positioning and rotation

# Binaries
Latest build for UE 4.25.1 available at http://research.m1stereo.tv/ue/FreeDLiveLink.7z


# Usage
To use this plugin you need enable LiveLink plugin in your project and unpack binaries to **Plugins** folder of your UE4 installation.

## Setup plugin.

Add FREED Live source:

![Add FREED Live source](/docs/cfg-0010.png?raw=true "Add FREED Live source")

Address in input string specify network interface plugin will listen to for incoming UDP packets with Free-D data. Address string mentioned above specify: ANY interface and port **20000** to listen.

After pressing **OK** button you will see a status of incoming datas:

![Incoming data status](/docs/cfg-0020.png?raw=true "Incoming data status")

Image above shows that plugin is receiving incoming datas. If you see nothing, then check if proper port specified and incoming UDP stream is present (i.e. your data source is present).

At this step plugin setup is compleate and you can follow next step.

## LiveLink data usage.

After plugin setup you need to assign a data stream it provides to an actor or it component.

In our example we will use **CineCameraActor** to control.

![target to control](/docs/cfg-0030.png?raw=true "target to control")

To assign LiveLink data to camera, you need to add a LiveLink controller component to your actor:

![LiveLink controller](/docs/cfg-0040.png?raw=true "LiveLink controller")

Setup LiveLink component to use data:

![Setup LiveLink component to use data](/docs/cfg-0050.png?raw=true "Setup LiveLink component to use data")

After selecting data stream, LiveLink will apply a transformation from that stream to Camera/Scene component of CineCamera you selected (does not mean which one you selected, because both accept *transfor*)

Another thing you need to keep in mind during configuration is Relative/World transformation been applied:

![World Transform](/docs/cfg-0060.png?raw=true "World Transform")

Uncheck option **World Transform** - you actor will have local Location/Rotation exact as data incoming with Free-D protocol.



