# LAB5

> 思考题 1: 文件的数据块使用基数树的形式组织有什么好处? 除此之外还有其他的数据块存储方式吗?

由于数据块存储多为字符串格式，而基数树能够实现快速搜索字符串，效率较高。于此同时他的拓展性也是较高，它能够调整树的节点结构来对文件大小进行适应。文件存储数据块除了使用基数树还能使用多级inode存储数据块，本质上就是线性存储，将文件的数据块按照顺序依次存储在磁盘上。

> 练习题 2：实现位于`userland/servers/tmpfs/tmpfs.c`的`tfs_mknod`和`tfs_namex`。

发现`chcore`中的lab内string还带了hash值，他的的结构包含`char*`,`len`和`hash`。并且`dentry`结构体目录中他的name的hash在`new_dent`函数中已经取好。`tfs_mknod`创建inode结点，根据`mkdir`来调用对应的创建函数`new_dir`或`new_reg`,并使用`htable_add`加入`parent dir`。

`tfs_namex`从`name`解析，遇到`"/"`便将在目录中查询对应的目录，如果`mkdir_p`为`true`,调用`tfs_mkdir`，否则返回INVALID

> 练习题 3：实现位于`userland/servers/tmpfs/tmpfs.c`的`tfs_file_read`和`tfs_file_write`。提示：由于数据块的大小为PAGE_SIZE，因此读写可能会牵涉到多个页面。读取不能超过文件大小，而写入可能会增加文件大小（也可能需要创建新的数据块）。

`tfs_file_write`根据offset计算对应的page number和page offset，然后找到radix tree中的数据页，通过memcpy拷贝至buffer中。

`tfs_file_read`相似，获取对应的pagenumber和offset，从radix树中获取page，如果为空说明文件已经读完，退出循环。

> 练习题 4：实现位于`userland/servers/tmpfs/tmpfs.c`的`tfs_load_image`函数。需要通过之前实现的tmpfs函数进行目录和文件的创建，以及数据的读写。

根据测试代码我们可以发现，`tmpfs_test`是在`tfs_load_image`基础上的，只有完成了`tfs_load_image`测试才会有分数。调用`tfs_namex`获取节点，调用`lookup`获取相应dent，如果dent为空则新建一个,将数据拷入

> 练习题 5：利用`userland/servers/tmpfs/tmpfs.c`中已经实现的函数，完成在`userland/servers/tmpfs/tmpfs_ops.c`中的`fs_creat`、`tmpfs_unlink`和`tmpfs_mkdir`函数，从而使`tmpfs_*`函数可以被`fs_server_dispatch`调用以提供系统服务。对应关系可以参照`userland/servers/tmpfs/tmpfs_ops.c`中`server_ops`的设置以及`userland/fs_base/fs_wrapper.c`的`fs_server_dispatch`函数。

调用对应的`tmpfs`函数即可。

> 练习题 6：补全`libchcore/src/libc/fs.c`与`libchcore/include/libc/FILE.h`文件，以实现`fopen`, `fwrite`, `fread`, `fclose`, `fscanf`, `fprintf`五个函数，函数用法应与libc中一致。

观察代码可以发现使用使用`ipc_call`进行转发即可。`fscanf`采用先read再解析的方法，`fprintf`利用chcore里本来就有的`simple_vprintf`来辅助解析。

> 练习题 7：实现在`userland/servers/shell/main.c`中定义的`getch`，该函数会每次从标准输入中获取字符，并实现在`userland/servers/shell/shell.c`中的`readline`，该函数会将按下回车键之前的输入内容存入内存缓冲区。代码中可以使用在`libchcore/include/libc/stdio.h`中的定义的I/O函数。

`getch`: 直接调用getc从串口读入一个字符。

`readline`: 反复调用getch读入字符，如果是`\t`则调用do_complement进行补全。

> 练习题 8：根据在`userland/servers/shell/shell.c`中实现好的`bultin_cmd`函数，完成shell中内置命令对应的`do_*`函数，需要支持的命令包括：`ls [dir]`、`echo [string]`、`cat [filename]`和`top`。

`do_ls`: 略过开头的ls两个字母与空格然后调用fs_scan。fs_scan中首先open对应的dir拿到fd，然后模仿demo_getdents打印出目录中的所有文件。

` do_echo`: 略过开头的echo四个字母与空格然后将cmdline打印即可。

`do_cat`: 通过`fread`来读取文件内容后print

> 练习题 9：实现在`userland/servers/shell/shell.c`中定义的`run_cmd`，以通过输入文件名来运行可执行文件，同时补全`do_complement`函数并修改`readline`函数，以支持按tab键自动补全根目录（`/`）下的文件名。

`do_complement`: 首先open根目录拿到fd，模仿demo_getdents

> 练习题 10：FSM需要两种不同的文件系统才能体现其特点，本实验提供了一个fakefs用于模拟部分文件系统的接口，测试代码会默认将tmpfs挂载到路径`/`，并将fakefs挂载在到路径`/fakefs`。本练习需要实现`userland/server/fsm/main.c`中空缺的部分，使得用户程序将文件系统请求发送给FSM后，FSM根据访问路径向对应文件系统发起请求，并将结果返回给用户程序。实现过程中可以使用`userland/server/fsm`目录下已经实现的函数。

通过代码阅读可以看出，`fsm.c`内提供了结构体`mount_point_info_node`,他提供了`  ipc_struct_t *_fs_ipc_struct;`，根据图所示，FSM层本质上要做的事情就是根据路径将请求转发到对应的文件系统中。在chcore中我们使用`ipc_call`来告知文件系统服务进程相对应的擦做。因此我们做的就是根据获得的request得到对应的``mount_point_info_node`,将request指令转发给`_fs_ipc_struct`server中，因为需要支持`ls`和`cat`指令，在shell中用到的函数为`FS_REQ_CREATE`,`FS_REQ_OPEN`,`FS_REQ_WRITE`,`FS_REQ_READ`,`FS_REQ_GETDENTS64`和`FS_REQ_CLOSE`。

