import os
import subprocess
import matplotlib.pyplot as plt

rb_insert, rb_find, rb_remove, st_insert, st_find, st_remove, time = [], [], [], [], [], [], []

if __name__ == "__main__":
    os.system("gcc rb_tree.c")
    for i in range(100000, 1000001, 100000):
        time.append(i)
        output = subprocess.run(["./a.out", str(i)], capture_output=True, text=True)
        lines = output.stdout.split("\n")
        rb_insert.append(int(lines[0].split(":")[1]))
        rb_find.append(int(lines[1].split(":")[1]))
        rb_remove.append(int(lines[2].split(":")[1]))
    
    os.system("gcc s_tree.c")
    for i in range(100000, 1000001, 100000):
        output = subprocess.run(["./a.out", str(i)], capture_output=True, text=True)
        lines = output.stdout.split("\n")
        st_insert.append(int(lines[0].split(":")[1]))
        st_find.append(int(lines[1].split(":")[1]))
        st_remove.append(int(lines[2].split(":")[1]))

    # plot insert
    plt.plot(time, rb_insert, label="rb_tree")
    plt.plot(time, st_insert, label="s_tree")
    plt.xlabel("tree size")
    plt.ylabel("time(us)")
    plt.title("insert")
    plt.legend()
    plt.ticklabel_format(style='plain')
    plt.savefig('./pic/insert.png')

    # plot find
    plt.clf()
    plt.plot(time, rb_find, label="rb_tree")
    plt.plot(time, st_find, label="s_tree")
    plt.xlabel("tree size")
    plt.ylabel("time(us)")
    plt.title("find")
    plt.legend()
    plt.ticklabel_format(style='plain')
    plt.savefig('./pic/find.png')

    # plot remove
    plt.clf()
    plt.plot(time, rb_remove, label="rb_tree")
    plt.plot(time, st_remove, label="s_tree")
    plt.xlabel("tree size")
    plt.ylabel("time(us)")
    plt.title("remove")
    plt.legend()
    plt.ticklabel_format(style='plain')
    plt.savefig('./pic/remove.png')