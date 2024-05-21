# Discharging

This program is used to prove Lemma regarding the discharging procedure. Specifically, it proves Lemma 5.5, 5.8, 5.11 in Section 5. The pseudo code of this program is described in Section C.3.

## Requirements
+ g++, CMake, Boost, spdlog

## Build
```bash
cmake -S . -B build
cmake --build build
```

## Execution
### Lemma 5.8, 5.11
We enumerate the cases where a vertex sends a charge.
We prepared a shell script (```enum_send.sh```), so we only execute the command below. 
```bash
bash enum_send.sh proj projective_configurations/rule projective_configurations/reducible/conf
```
More detailed information is in ```enum_send.sh```.

### Lemma 5.5
We use the results of ```enum_send.sh``` to prove Lemma 5.5, so first we have to execute the above command. After that, we need three steps to get the results.

1. enumerate wheels.\
We prepared a shell script (```enum_wheel.sh```), so we only execute the commands below. It takes long time to generate wheel whose hub's degree is 10, or 11.
```bash
bash enum_wheel.sh proj 7 projective_configurations/reducible/conf
bash enum_wheel.sh proj 8 projective_configurations/reducible/conf
bash enum_wheel.sh proj 9 projective_configurations/reducible/conf
bash enum_wheel.sh proj 10 projective_configurations/reducible/conf
bash enum_wheel.sh proj 11 projective_configurations/reducible/conf
```
More detailed information is in ```enum_wheel.sh```.

2. exectute the discharging procedure to each wheel.\
We prepared a shell script (```discharge.sh```).
There are many wheel files in ```proj_wheel```, which are enumerated in step 1, so we cannot execute the discharging procedure to all wheels at once. We run the shell script in several batches. An example is the following.
```bash
bash discharge.sh proj 7 0 1500 projective_configurations/rule projective_configurations/reducible/conf
bash discharge.sh proj 7 1501 3000 projective_configurations/rule projective_configurations/reducible/conf
bash discharge.sh proj 7 3001 4500 projective_configurations/rule projective_configurations/reducible/conf
bash discharge.sh proj 7 4501 4703 projective_configurations/rule projective_configurations/reducible/conf
```
We run the shell script when the degree is 8,9,10,11 in the similar way. More detailed information is in ```discharge.sh```

3. make sure the charge of the hub of all wheels is at most 0.
We prepared the shell script (```charge_result.sh```). so we only execute the commands below.
```bash
bash charge_result.sh proj 7 out_proj7.txt
bash charge_result.sh proj 8 out_proj8.txt
bash charge_result.sh proj 9 out_proj9.txt
bash charge_result.sh proj 10 out_proj10.txt
bash charge_result.sh proj 11 out_proj11.txt
```
More detailed information is in ```charge_result.sh```.

## Results
The directory ```proj_send```,```proj_wheel``` already contains the results. All cases that a vertex sends a charge are enumearted in ```proj_send```. The drawings of them are also placed in ```proj_send_pdf``` (the cases that a vertex sends charge $N$ are drawen in ```proj_send_pdf/sendN.pdf```).  All wheels are enumerated in ```proj_wheel```. The directory ```proj_log``` contains one of the results (the result of ```./proj_wheel/7_0.wheel```). The result shows the successful log of discharging check. If there is no overcharged cartwheel (```the ratio of overcharged carthwheel 0``` appears in log), it is successful. Please check it if you are interested.

## File Format
### Configuration File Format
A file whose extension is ```.conf``` represents a configuration. The symbol ```N, R``` denotes the size of vertices of the free completion with the ring, the size of the ring respectively. ```v_{R+1}, v_{R+2}, ..., v_N``` denote the vertices of a configuration.

```
dummy-line
N R
R+1 (The degree of v_{R+1}) (The list of neighbors of v_{R+1}) 
R+2 (The degree of v_{R+2}) (The list of neighbors of v_{R+2})
...
N (The degree of v_N) (The list of neighbors of v_N)
```
An example.
```
001
10 6
7 5 2 8 9 10 1
8 5 2 3 4 9 7
9 5 8 4 5 10 7
10 5 9 5 6 1 7
```

### Wheel File Format
A file whose extension is ```.wheel``` represents a wheel.
```v_1, v_2, ...``` denotes the neighbors of the hub in clockwise order.

```
(The degree of the hub) (The degree of v_1) (The degree of v_2) ... 
```
An example
```
7 5 6 6 5 6 8+ 9+
```

### Rule File Format
A file whose extension is ```.rule``` represents a rule. The symbol ```N, s, t, r``` represents the size of vertices, the vertex that sends charge, the vertex that receives charge, the amount of charge sent respectively. ```v_1, v_2,..., v_N``` denote the vertices of a rule

```
dummy-line
N s t r
1 (The degree of v_1) (The list of neighbors of v_1)
2 (The degree of v_2) (The list of neighbors of v_2)
...
N (The degree of v_N) (The list of neighbors of v_N)
```
An example (rule1).
```
rule1
2 1 2 2
1 5  2
2 5+ 1
```