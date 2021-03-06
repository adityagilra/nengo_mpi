from __future__ import print_function

import os
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import seaborn
seaborn.set(style="white")
seaborn.set_context(rc={'lines.markeredgewidth': 0.1})
from matplotlib.markers import MarkerStyle
import subprocess
import shutil
import re
import time
from doit import get_var

from utils import write_to_csv

# Modify these by supplying key=val at the end of a doit-command
params = {
    'n': get_var('n', ''),
    't': get_var('t', 1.0),
    'seed': get_var('seed', 10),
    'n_copies': get_var('n_copies', 1),
    'max_procs': get_var('max_procs', 8)}


network = params['n']
t = float(params['t'])
seed = int(params['seed'])
n_copies = int(params['n_copies'])
max_procs = int(params['max_procs'])

bench_loc = os.getenv('NENGO_MPI_BENCH_HOME')
nengo_mpi_loc = os.path.join(os.getenv("HOME"), "nengo_mpi")
print ("NENGO_MPI_BENCH_HOME: %s" % bench_loc)

assert network, "Network arg must be supplied."


def task_create():
    loc = os.path.join(bench_loc, network)
    if not os.path.isdir(loc):
        os.makedirs(loc)

    def f(command):
        output = subprocess.check_output(
            command.split(' '))
        print(output)

    if network == 'grid':
        n_procs = [1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096]
        n_ensembles = [16, 64, 256, 1024, 4096]

        command_template = (
            "python grid.py --sl {sl} --ns {ns} -d 4 --npd 50 "
            "--mpi 1 -p {n_procs} --save {save} --seed {seed} "
            "--pfunc metis")
        save_template = "grid_p{n_procs}_e{n_ensembles}_c{copy}.net"

        for i in n_procs:
            for j in n_ensembles:
                dir_name = "procs{procs}_ens{ens}".format(procs=i, ens=j)
                dir_name = os.path.join(loc, dir_name)

                if not os.path.isdir(dir_name):
                    os.makedirs(dir_name)

                if j >= i:
                    for k in range(n_copies):
                        sqrtj = int(np.sqrt(j))
                        save = save_template.format(
                            n_procs=i, n_ensembles=j, copy=k)

                        command = command_template.format(
                            sl=sqrtj, ns=sqrtj, n_procs=i,
                            save=os.path.join(dir_name, save),
                            seed=seed+k)

                        yield {'name': save,
                               'actions': [(f, [command],)],
                               'targets': [save]}

    elif network == 'random':
        command_template = (
            "python random_graph.py -n {n_ensembles} -d 4 --npd 50 -q {q} "
            "--mpi 1 -p {n_procs} --save {save} --seed {seed} "
            "--pfunc metis --probes 0.2 --fake --ea")
        save_template = "random_p{n_procs}_e{n_ensembles}_q{q}_c{copy}.net"

        n_ensembles = 1024
        n_procs = [1, 8, 64, 512, 4096]
        q_vals = np.linspace(0.01, 0.05, 5)

        for i in n_procs:
            for q in q_vals:
                dir_name = "q{q}_procs{procs}_ens{ens}".format(
                    q=int(q*1000), procs=i, ens=n_ensembles)
                dir_name = os.path.join(loc, dir_name)

                if not os.path.isdir(dir_name):
                    os.makedirs(dir_name)

                if n_ensembles >= i:
                    for k in range(n_copies):
                        save = save_template.format(
                            n_procs=i, n_ensembles=n_ensembles,
                            q=int(q*1000), copy=k)

                        command = command_template.format(
                            n_ensembles=n_ensembles, q=q,
                            n_procs=i, seed=seed+k,
                            save=os.path.join(dir_name, save))

                        yield {'name': save,
                               'actions': [(f, [command],)],
                               'targets': [save]}
    else:
        raise NotImplementedError()


def task_runsa():
    def f(network, t, seed, max_procs):
        loc = os.path.join(bench_loc, network)
        if not os.path.isdir(loc):
            os.makedirs(loc)

        command_template = "mpirun -np {nprocs} nengo_mpi {netfile} {t}"
        pattern = "{0}_p(?P<p>.+)_e(?P<e>.+)_c(?P<c>.+).net".format(network)

        os.environ['NENGO_MPI_LOADTIMES_FILE'] = os.path.join(loc, "loadtimes.csv")
        os.environ['NENGO_MPI_RUNTIMES_FILE'] = os.path.join(loc, "runtimes.csv")

        for d in os.listdir(loc):
            dir_path = os.path.join(loc, d)
            if not os.path.isdir(dir_path):
                continue

            for netfile in os.listdir(dir_path):
                m = re.match(pattern, netfile)
                if m:
                    n_procs = int(m.groupdict()['p'])

                    if n_procs <= max_procs:
                        print("** Running %s" % netfile)
                        netfile_path = os.path.join(dir_path, netfile)
                        command = command_template.format(
                            nprocs=n_procs, netfile=netfile_path, t=t)

                        t0 = time.time()
                        output = subprocess.check_output(command.split(' '))
                        t1 = time.time()

                        print(output)
                        print("** The command took " + str(t1 - t0) + " seconds.")

                        vals = dict(
                            runtimes=t1-t0, label=netfile,
                            t=t, nprocs=n_procs)
                        runtimes_file = os.path.join(loc, 'total_runtimes.csv')
                        write_to_csv(runtimes_file, vals)
                    else:
                        print("** Ignoring %s because maxprocs (%s) "
                              "exceeded" % (netfile, max_procs))

                else:
                    print("** Ignoring non-network %s" % netfile)

    return {'actions': [(f, [network, t, seed, max_procs],)],
            'verbosity': 2}


def task_cleannets():
    def f(network):
        path = os.path.join(bench_loc, network)
        try:
            shutil.rmtree(path)
        except OSError as e:
            print(e)

    return {'actions': [(f, [network],)],
            'verbosity': 2}


def task_cleantimes():
    def f(network):
        path = os.path.join(bench_loc, network)
        if not os.path.isdir(path):
            os.makedirs(path)

        for f in os.listdir(path):
            ff = os.path.join(path, f)
            if not os.path.isdir(ff):
                print("** Removing %s" % ff)
                os.remove(ff)

    return {'actions': [(f, [network],)],
            'verbosity': 2}


def task_plot():
    def plot(
            save, path, xlabel, ylabel, title, fn_pattern,
            ind_var_name, do_log=True, realtime=None):

        plt.figure()
        df = pd.read_csv(path)
        ind_var = [re.match(fn_pattern, s).groups()[0] for s in df.label]
        df[ind_var_name] = ind_var
        mean_runtime = df.groupby(['nprocs', ind_var_name])['seconds'].mean()
        if realtime:
            plt.axhline(
                realtime, color='k', linestyle='dashed', label="Real Time")

        xticks = set()
        plot_params = ['-'+m for m in MarkerStyle.filled_markers]
        for p, n_procs in zip(plot_params, mean_runtime.index.levels[0]):
            s = mean_runtime.loc[n_procs]
            s = pd.Series(data=list(s), index=[int(n) for n in s.index])
            s = s.sort_index()

            x = list(s.index)
            y = list(s)

            xticks |= set(x)

            if do_log:
                plt.loglog(
                    x, y, p, label="%d proc" % int(n_procs),
                    basex=2, basey=10)
            else:
                plt.plot(x, y, p, label="%d proc" % int(n_procs))

        if do_log:
            minx = min(xticks)
            maxx = max(xticks)

            plt.xlim((2**(np.log2(minx)-0.5), 2**(np.log2(maxx)+0.5)))

        plt.xlabel(xlabel)
        plt.ylabel(ylabel)
        plt.title(title)

        plt.legend(
            loc='center left', bbox_to_anchor=(1, 0.5), prop={'size': 10},
            handlelength=2.0, handletextpad=.5, shadow=False, frameon=False)
        plt.subplots_adjust(left=0.2, right=0.8, top=0.9, bottom=0.1)

        plt.savefig(save)

    params = {
        'bgq_grid_run': dict(
            save='plots/bgq_grid_run.pdf',
            path="results/bgq_grid/runtimes.csv",
            xlabel="# of Ensembles",
            ylabel="Run time (s)",
            title="Grid Network on BGQ",
            do_log=True,
            fn_pattern="grid_p\d+_e(\w+)_c0.net",
            ind_var_name="n_ens",
            realtime=5.0),
        'bgq_grid_load': dict(
            save='plots/bgq_grid_load.pdf',
            path="results/bgq_grid/loadtimes.csv",
            xlabel="# of Ensembles",
            ylabel="Load time (s)",
            title="Grid Network on BGQ",
            do_log=True,
            fn_pattern="grid_p\d+_e(\w+)_c0.net",
            ind_var_name="n_ens",
            realtime=None),
    }

    for name, kwargs in params.iteritems():
        yield {
            'name': name,
            'actions': [(plot, [], kwargs)],
            'verbosity': 2}
