# Lab6

> 思考题 1：请自行查阅资料，并阅读`userland/servers/sd`中的代码，回答以下问题:
>
> - circle中还提供了SDHost的代码。SD卡，EMMC和SDHost三者之间的关系是怎么样的？
> - 请**详细**描述Chcore是如何与SD卡进行交互的？即Chcore发出的指令是如何输送到SD卡上，又是如何得到SD卡的响应的。(提示: IO设备常使用MMIO的方式映射到内存空间当中)
> - 请简要介绍一下SD卡驱动的初始化流程。
> - 在驱动代码的初始化当中，设置时钟频率的意义是什么？为什么需要调用`TimeoutWait`进行等待?

Chcore调用`libchcore`中的代码，通过`ipc`来将发出的指令传递给sd卡的`ipc_server`，sd卡的`ipc_server`接收到发出的`ipc_msg`后，调用`sdcard_writeblock`或是调用`sdcard_readblock`函数。采用`mmio`映射将设备地址映射到内存空间当中。`Chcore`获取数据后，会将数组传给`DoDataCommand`，指定对应的参数，读取多块单块还是写多块单块，并重试传递到`IssueCommand`函数。

在`IssueCommandInt`函数中`Chcore`先通过写入先前映射的物理地址来给sd卡传参，这个地址既是内存地址又是设备本身的地址，传递`EMMC_ARG1`参数和`EMMC_CMDTM`cmd指令，后等待sd卡硬件实施，sd卡完成后会给驱动发送中断，`Chcore`收到中断后读取sd卡的寄存器，也就是在固定位置上的物理地址值读取32位`EMMC_DATA`,关键代码如下：

```c
 write32(EMMC_BLKSIZECNT, blksizecnt);

 // Set argument 1 reg
 write32(EMMC_ARG1, argument);

 // Set command reg
 write32(EMMC_CMDTM, cmd_reg);

// ...
					if (is_write) {
                        for (; length > 0; length -= 4) {
                                        write32(EMMC_DATA, *pData++);
                                }
                        } else {
                                for (; length > 0; length -= 4) {
                                        *pData++ = read32(EMMC_DATA);
                                }
                        }
```

SD卡驱动的初始化流程首先是通过`map_mmio`映射，后启动sd卡并获取设备信息，后注册`ipc_server`接受来自`Chcore`的`ipc`请求

在驱动代码的初始化中设置时钟频率的意义是确定设备的工作速率。时钟频率决定了设备的数据传输速度、中断触发频率等重要参数。

调用`TimeoutWait`进行等待的目的是确保设备在初始化过程中完成必要的操作，例如初始化寄存器、加载固件等。在初始化过程中，设备可能需要一些时间来完成这些操作。`TimeoutWait`函数用于等待设备完成这些操作，以确保设备处于正确的状态。

> 练习 1：完成`userland/servers/sd`中的代码，实现SD卡驱动。驱动程序需实现为用户态系统服务，应用程序与驱动通过 IPC 通信。需要实现 `sdcard_readblock` 与 `sdcard_writeblock` 接口，通过 Logical Block Address(LBA) 作为参数访问 SD 卡的块。

我们可以参考[circle](https://github.com/rsta2/circle/tree/master/addon/SDCard)中的代码实现来实现`sd_Read`和`sd_Write`，同时我们注意到我们需要同时实现LBA，这时候就可以用到`emmc.c`中提供的`Seek`函数，我们将sd卡按`BLOCK_SIZE`划分为逻辑块，对应的lba就`Seek`到对应处来读取。

