#!/usr/bin/env python

'''
This uses both posture and orientation control.
'''

import sys, getopt     # for getting and parsing command line arguments
import time
import math
import threading
import rospy

from std_msgs.msg import Float64, Float64MultiArray, MultiArrayDimension, Bool, Int32

import numpy as np
from scipy.interpolate import interp1d
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D

import DreamerInterface
import Trajectory
import TrajectoryGeneratorCubicSpline

ENABLE_USER_PROMPTS = False

# Shoulder abductors about 10 degrees away from body and elbows bent 90 degrees
# DEFAULT_POSTURE = [0.0, 0.0,                                    # torso
#                    0.0, 0.174532925, 0.0, 1.57, 0.0, 0.0, 0.0,  # left arm
#                    0.0, 0.174532925, 0.0, 1.57, 0.0, 0.0, 0.0]  # right arm

# Shoulder abductors and elbows at about 10 degrees
DEFAULT_POSTURE = [0.0, 0.0,                                    # torso
                   0.0, 0.174532925, 0.0, 0.174532925, 0.0, 0.0, 0.0,  # left arm
                   0.0, 0.174532925, 0.0, 0.174532925, 0.0, 0.0, 0.0]  # right arm

class Demo4_HandWave:
    def __init__(self):
        self.dreamerInterface = DreamerInterface.DreamerInterface(ENABLE_USER_PROMPTS)

    def createTrajectories(self):

        # ==============================================================================================
        # Define the GoToReady trajectory
        self.trajGoToReady = Trajectory.Trajectory("GoToReady", 5.0)

        # These are the initial values as specified in the YAML ControlIt! configuration file
        self.trajGoToReady.setInitRHCartWP([0.033912978219317776, -0.29726881641499886, 0.82])
        self.trajGoToReady.setInitLHCartWP([0.033912978219317776, 0.29726881641499886, 0.82])
        self.trajGoToReady.setInitRHOrientWP([1.0, 0.0, 0.0])
        self.trajGoToReady.setInitLHOrientWP([1.0, 0.0, 0.0])
        self.trajGoToReady.setInitPostureWP(DEFAULT_POSTURE)
        
        self.trajGoToReady.addRHCartWP([0.25342182518741946, -0.23249440323488493, 0.8583425239604793])
        self.trajGoToReady.addRHCartWP([0.36027175395810024, -0.2366487226876633, 0.9970636893031656])
        self.trajGoToReady.addRHCartWP([0.4186832833062845, -0.23161337689921324, 1.195102104388137])
        self.trajGoToReady.addRHCartWP([0.4204761320567268, -0.22276691579626728, 1.3128166611913739])
        self.trajGoToReady.addRHCartWP([0.40394280229724117, -0.21627961864869003, 1.360149033190145])

        self.trajGoToReady.addRHOrientWP([0.6730021599472973, 0.031757024689336236, 0.7389584454413883])
        self.trajGoToReady.addRHOrientWP([0.08082529884000572, 0.0531921675651184, 0.9953079244018648])
        self.trajGoToReady.addRHOrientWP([-0.603301024048348, -0.06831640854719574, 0.794582118289499])
        self.trajGoToReady.addRHOrientWP([-0.8358379442751223, -0.15573948591246203, 0.5264220202818829])
        self.trajGoToReady.addRHOrientWP([-0.9086099905348223, -0.1356275807488986, 0.39501018270484023])

        self.trajGoToReady.addLHCartWP([0.25342182518741946, 0.23249440323488493, 0.8583425239604793])
        self.trajGoToReady.addLHCartWP([0.36027175395810024, 0.2366487226876633, 0.9970636893031656])
        self.trajGoToReady.addLHCartWP([0.4186832833062845, 0.23161337689921324, 1.195102104388137])
        self.trajGoToReady.addLHCartWP([0.4204761320567268, 0.22276691579626728, 1.3128166611913739])
        self.trajGoToReady.addLHCartWP([0.40394280229724117, 0.21627961864869003, 1.360149033190145])

        self.trajGoToReady.addLHOrientWP([0.6730021599472973, 0.031757024689336236, 0.7389584454413883])
        self.trajGoToReady.addLHOrientWP([0.08082529884000572, 0.0531921675651184, 0.9953079244018648])
        self.trajGoToReady.addLHOrientWP([-0.603301024048348, -0.06831640854719574, 0.794582118289499])
        self.trajGoToReady.addLHOrientWP([-0.8358379442751223, -0.15573948591246203, 0.5264220202818829])
        self.trajGoToReady.addLHOrientWP([-0.9086099905348223, -0.1356275807488986, 0.39501018270484023])

        self.trajGoToReady.addPostureWP([0.09559710847417426, 0.09559710847417426, 
            0.18493035171345457, -0.018093572235340995, 0.12375741975377275, 0.7804877049806426, -0.12790804246025334, 0.06510619728499865, -0.04870942823921384,
            0.18493035171345457, -0.018093572235340995, 0.12375741975377275, 0.7804877049806426, -0.12790804246025334, 0.06510619728499865, -0.04870942823921384])
        self.trajGoToReady.addPostureWP([0.09643131954418892, 0.09643131954418892,
            0.2937028050698316, -0.03443394290863798, 0.1228407406394784, 1.2509975562933466, -0.10977267724909819, 0.14852369547245972, -0.11381649195385224,
            0.2937028050698316, -0.03443394290863798, 0.1228407406394784, 1.2509975562933466, -0.10977267724909819, 0.14852369547245972, -0.11381649195385224])
        self.trajGoToReady.addPostureWP([0.09592785524414665, 0.09592785524414665,
            0.581393160586458, -0.06765800596877417, 0.11796117340291093, 1.6240955805165236, 0.03394658862046966, 0.20374019828081738, -0.1572576379385714,
            0.581393160586458, -0.06765800596877417, 0.11796117340291093, 1.6240955805165236, 0.03394658862046966, 0.20374019828081738, -0.1572576379385714])
        self.trajGoToReady.addPostureWP([0.09631485379135454, 0.09631485379135454,
            0.8297652931556753, -0.07639062384459042, 0.08683982306492016, 1.743970293595999, 0.11813285955434884, 0.18560978107628873, -0.1624212287078026,
            0.8297652931556753, -0.07639062384459042, 0.08683982306492016, 1.743970293595999, 0.11813285955434884, 0.18560978107628873, -0.1624212287078026])
        self.trajGoToReady.addPostureWP([0.09670000331985308, 0.09670000331985308,
            0.9240362038181721, -0.076429908585699, 0.058531084397708745, 1.821464996691965, 0.09191340335475721, 0.17154826940628795, -0.15081068818347018,
            0.9240362038181721, -0.076429908585699, 0.058531084397708745, 1.821464996691965, 0.09191340335475721, 0.17154826940628795, -0.15081068818347018])


        # ==============================================================================================
        self.trajWave = Trajectory.Trajectory("Wave", 5.0)
        self.trajWave.setPrevTraj(self.trajGoToReady)

        self.trajWave.addRHCartWP([0.36485155544112036, -0.2517840242514848, 1.3112985442583098])
        self.trajWave.addRHCartWP([0.34112607633989245, -0.3058363428211823, 1.3153096901049126])
        self.trajWave.addRHCartWP([0.3189402691299889, -0.3648415066801503, 1.2686035469564463])
        self.trajWave.addRHCartWP([0.27834518368887395, -0.40498666141553075, 1.2197560223120059])
        self.trajWave.addRHCartWP([0.2894866074052446, -0.37589494706165555, 1.277789646796522])
        self.trajWave.addRHCartWP([0.3054862120250459, -0.3109708763630136, 1.307510639671462])
        self.trajWave.addRHCartWP([0.31592102945392736, -0.26786592118202396, 1.315217874025364])
        self.trajWave.addRHCartWP([0.31873722293582485, -0.22050165190244836, 1.3077860592406567])
        self.trajWave.addRHCartWP([0.3140206375491238, -0.1808165328946215, 1.2927024975528303])
        self.trajWave.addRHCartWP([0.3173188478462051, -0.15584603682048545, 1.2703563812275773])
        self.trajWave.addRHCartWP([0.329389782035803, -0.20211525876502226, 1.2871299455725096])
        self.trajWave.addRHCartWP([0.32994847638345126, -0.2257724457825943, 1.282110810745384])

        self.trajWave.addRHOrientWP([-0.8483957340932968, -0.06311943166472188, 0.5255859736699781])
        self.trajWave.addRHOrientWP([-0.9172289926078292, -0.12475056459465815, 0.3783229728062264])
        self.trajWave.addRHOrientWP([-0.9213984789882482, -0.17117565050862832, 0.34888929417666364])
        self.trajWave.addRHOrientWP([-0.9290495561053892, -0.11893058361794344, 0.3503176252783671])
        self.trajWave.addRHOrientWP([-0.9272636764282779, -0.1573054986207779, 0.339760289734369])
        self.trajWave.addRHOrientWP([-0.8955302967285547, -0.13717216693581574, 0.4233311756289626])
        self.trajWave.addRHOrientWP([-0.8557528439776211, -0.17501147944063347, 0.48688607711477466])
        self.trajWave.addRHOrientWP([-0.7872868383457702, -0.18112481164989888, 0.589383777153995])
        self.trajWave.addRHOrientWP([-0.72501212928327, -0.12618470059651335, 0.6770781592456718])
        self.trajWave.addRHOrientWP([-0.7220611506203211, -0.10965176413652575, 0.6830843179188101])
        self.trajWave.addRHOrientWP([-0.7761597782901096, -0.09693054071636907, 0.6230413058867605])
        self.trajWave.addRHOrientWP([-0.7702887617718236, -0.10857860501605597, 0.6283835691842989])

        self.trajWave.addLHCartWP([0.36485155544112036, 0.2517840242514848, 1.3112985442583098])
        self.trajWave.addLHCartWP([0.34112607633989245, 0.3058363428211823, 1.3153096901049126])
        self.trajWave.addLHCartWP([0.3189402691299889, 0.3648415066801503, 1.2686035469564463])
        self.trajWave.addLHCartWP([0.27834518368887395, 0.40498666141553075, 1.2197560223120059])
        self.trajWave.addLHCartWP([0.2894866074052446, 0.37589494706165555, 1.277789646796522])
        self.trajWave.addLHCartWP([0.3054862120250459, 0.3109708763630136, 1.307510639671462])
        self.trajWave.addLHCartWP([0.31592102945392736, 0.26786592118202396, 1.315217874025364])
        self.trajWave.addLHCartWP([0.31873722293582485, 0.22050165190244836, 1.3077860592406567])
        self.trajWave.addLHCartWP([0.3140206375491238, 0.1808165328946215, 1.2927024975528303])
        self.trajWave.addLHCartWP([0.3173188478462051, 0.15584603682048545, 1.2703563812275773])
        self.trajWave.addLHCartWP([0.329389782035803, 0.20211525876502226, 1.2871299455725096])
        self.trajWave.addLHCartWP([0.32994847638345126, 0.2257724457825943, 1.282110810745384])

        self.trajWave.addLHOrientWP([-0.8483957340932968, -0.06311943166472188, 0.5255859736699781])
        self.trajWave.addLHOrientWP([-0.9172289926078292, -0.12475056459465815, 0.3783229728062264])
        self.trajWave.addLHOrientWP([-0.9213984789882482, -0.17117565050862832, 0.34888929417666364])
        self.trajWave.addLHOrientWP([-0.9290495561053892, -0.11893058361794344, 0.3503176252783671])
        self.trajWave.addLHOrientWP([-0.9272636764282779, -0.1573054986207779, 0.339760289734369])
        self.trajWave.addLHOrientWP([-0.8955302967285547, -0.13717216693581574, 0.4233311756289626])
        self.trajWave.addLHOrientWP([-0.8557528439776211, -0.17501147944063347, 0.48688607711477466])
        self.trajWave.addLHOrientWP([-0.7872868383457702, -0.18112481164989888, 0.589383777153995])
        self.trajWave.addLHOrientWP([-0.72501212928327, -0.12618470059651335, 0.6770781592456718])
        self.trajWave.addLHOrientWP([-0.7220611506203211, -0.10965176413652575, 0.6830843179188101])
        self.trajWave.addLHOrientWP([-0.7761597782901096, -0.09693054071636907, 0.6230413058867605])
        self.trajWave.addLHOrientWP([-0.7702887617718236, -0.10857860501605597, 0.6283835691842989])


        self.trajWave.addPostureWP([0.09613784518172848, 0.09613784518172848, 
            0.6963737388453927, -0.0253518555307052, 0.15341789323600494, 1.9795791803014502, 0.1158415127482893, 0.09139178435445318, -0.12501872906423647,
            0.6963737388453927, -0.0253518555307052, 0.15341789323600494, 1.9795791803014502, 0.1158415127482893, 0.09139178435445318, -0.12501872906423647])
        self.trajWave.addPostureWP([0.09625555521112346, 0.09625555521112346, 
            0.7304879368095207, -0.06899682475443332, 0.40789164898420543, 2.0355279378543747, 0.30583848821360793, 0.10344976842444814, -0.10910862698864073,
            0.7304879368095207, -0.06899682475443332, 0.40789164898420543, 2.0355279378543747, 0.30583848821360793, 0.10344976842444814, -0.10910862698864073])
        self.trajWave.addPostureWP([0.09648391059258273, 0.09648391059258273, 
            0.6872987058986008, -0.1003624415982052, 0.6988371319143283, 1.9956590078370402, 0.5126819306296545, 0.08176581451472757, -0.08751496031292608,
            0.6872987058986008, -0.1003624415982052, 0.6988371319143283, 1.9956590078370402, 0.5126819306296545, 0.08176581451472757, -0.08751496031292608])
        self.trajWave.addPostureWP([0.09623584827226966, 0.09623584827226966, 
            0.6087661656488711, -0.11384257144836213, 0.9357664921949188, 1.975477903314159, 0.6372117664424289, 0.07979401227238916, -0.09204535169626671,
            0.6087661656488711, -0.11384257144836213, 0.9357664921949188, 1.975477903314159, 0.6372117664424289, 0.07979401227238916, -0.09204535169626671])
        self.trajWave.addPostureWP([0.09605107006155901, 0.09605107006155901, 
            0.5903983904272552, 0.005661412241148953, 0.6940595164957042, 2.074124356844139, 0.6288091831025431, 0.07275434702891888, -0.09733654197774945,
            0.5903983904272552, 0.005661412241148953, 0.6940595164957042, 2.074124356844139, 0.6288091831025431, 0.07275434702891888, -0.09733654197774945])
        self.trajWave.addPostureWP([0.09587305832559002, 0.09587305832559002, 
            0.5719566833137808, 0.05376116233926953, 0.38175944515758553, 2.1565377924785256, 0.42874387966331046, 0.07104095078071092, -0.09899430839029684,
            0.5719566833137808, 0.05376116233926953, 0.38175944515758553, 2.1565377924785256, 0.42874387966331046, 0.07104095078071092, -0.09899430839029684])
        self.trajWave.addPostureWP([0.09557756158984006, 0.09557756158984006, 
            0.5667624908623723, 0.12511107275805083, 0.15596429138242857, 2.162860038448929, 0.37115046969928445, 0.059544480396001076, -0.10307352006180331,
            0.5667624908623723, 0.12511107275805083, 0.15596429138242857, 2.162860038448929, 0.37115046969928445, 0.059544480396001076, -0.10307352006180331])
        self.trajWave.addPostureWP([0.09574236113985062, 0.09574236113985062, 
            0.5520121965874973, 0.22300468689009542, -0.09799331602657664, 2.16316752062064, 0.31181680470157863, 0.03691966992672197, -0.12373090857848369,
            0.5520121965874973, 0.22300468689009542, -0.09799331602657664, 2.16316752062064, 0.31181680470157863, 0.03691966992672197, -0.12373090857848369])
        self.trajWave.addPostureWP([0.0957804860207419, 0.0957804860207419, 
            0.556780400008231, 0.3065317489943526, -0.3246761329975119, 2.1578362934156603, 0.23213210844055276, -0.0026838663766573317, -0.14708447815273884,
            0.556780400008231, 0.3065317489943526, -0.3246761329975119, 2.1578362934156603, 0.23213210844055276, -0.0026838663766573317, -0.14708447815273884])
        self.trajWave.addPostureWP([0.09627960142249233, 0.09627960142249233, 
            0.5202609540435355, 0.22979463495835548, -0.39326924708232214, 2.112970559628145, 0.10613743059875844, 0.03890155837137803, -0.06953474833585839,
            0.5202609540435355, 0.22979463495835548, -0.39326924708232214, 2.112970559628145, 0.10613743059875844, 0.03890155837137803, -0.06953474833585839])
        self.trajWave.addPostureWP([0.0963633996624976, 0.0963633996624976, 
            0.5207676650989186, 0.08567702368999285, -0.10652871175134726, 2.1155806117429234, 0.11070397260881194, 0.040478077003761374, -0.06553297119228002,
            0.5207676650989186, 0.08567702368999285, -0.10652871175134726, 2.1155806117429234, 0.11070397260881194, 0.040478077003761374, -0.06553297119228002])
        self.trajWave.addPostureWP([0.09612037607769207, 0.09612037607769207, 
            0.5027854641176872, 0.009744544772433712, 0.033240697198917396, 2.114519650035102, 0.13509430821702553, 0.02904191010039892, -0.05842000907623836,
            0.5027854641176872, 0.009744544772433712, 0.033240697198917396, 2.114519650035102, 0.13509430821702553, 0.02904191010039892, -0.05842000907623836])


        # ==============================================================================================        
        self.trajGoToIdle = Trajectory.Trajectory("GoToIdle", 5.0)
        self.trajGoToIdle.setPrevTraj(self.trajWave)

        self.trajGoToIdle.addRHCartWP([0.40394280229724117, -0.21627961864869003, 1.360149033190145])
        self.trajGoToIdle.addRHCartWP([0.4204761320567268, -0.22276691579626728, 1.3128166611913739])
        self.trajGoToIdle.addRHCartWP([0.4186832833062845, -0.23161337689921324, 1.195102104388137])
        self.trajGoToIdle.addRHCartWP([0.36027175395810024, -0.2366487226876633, 0.9970636893031656])
        self.trajGoToIdle.addRHCartWP([0.25342182518741946, -0.23249440323488493, 0.8583425239604793])

        self.trajGoToIdle.addRHOrientWP([-0.9086099905348223, -0.1356275807488986, 0.39501018270484023])
        self.trajGoToIdle.addRHOrientWP([-0.8358379442751223, -0.15573948591246203, 0.5264220202818829])
        self.trajGoToIdle.addRHOrientWP([-0.603301024048348, -0.06831640854719574, 0.794582118289499])
        self.trajGoToIdle.addRHOrientWP([0.08082529884000572, 0.0531921675651184, 0.9953079244018648])
        self.trajGoToIdle.addRHOrientWP([0.6730021599472973, 0.031757024689336236, 0.7389584454413883])

        self.trajGoToIdle.addLHCartWP([0.40394280229724117, 0.21627961864869003, 1.360149033190145])
        self.trajGoToIdle.addLHCartWP([0.4204761320567268, 0.22276691579626728, 1.3128166611913739])
        self.trajGoToIdle.addLHCartWP([0.4186832833062845, 0.23161337689921324, 1.195102104388137])
        self.trajGoToIdle.addLHCartWP([0.36027175395810024, 0.2366487226876633, 0.9970636893031656])
        self.trajGoToIdle.addLHCartWP([0.25342182518741946, 0.23249440323488493, 0.8583425239604793])

        self.trajGoToIdle.addLHOrientWP([-0.9086099905348223, -0.1356275807488986, 0.39501018270484023])
        self.trajGoToIdle.addLHOrientWP([-0.8358379442751223, -0.15573948591246203, 0.5264220202818829])
        self.trajGoToIdle.addLHOrientWP([-0.603301024048348, -0.06831640854719574, 0.794582118289499])
        self.trajGoToIdle.addLHOrientWP([0.08082529884000572, 0.0531921675651184, 0.9953079244018648])
        self.trajGoToIdle.addLHOrientWP([0.6730021599472973, 0.031757024689336236, 0.7389584454413883])

        self.trajGoToIdle.addPostureWP([0.09670000331985308, 0.09670000331985308, 
            0.9240362038181721, -0.076429908585699, 0.058531084397708745, 1.821464996691965, 0.09191340335475721, 0.17154826940628795, -0.15081068818347018,
            0.9240362038181721, -0.076429908585699, 0.058531084397708745, 1.821464996691965, 0.09191340335475721, 0.17154826940628795, -0.15081068818347018])
        self.trajGoToIdle.addPostureWP([0.09631485379135454, 0.09631485379135454, 
            0.8297652931556753, -0.07639062384459042, 0.08683982306492016, 1.743970293595999, 0.11813285955434884, 0.18560978107628873, -0.1624212287078026,
            0.8297652931556753, -0.07639062384459042, 0.08683982306492016, 1.743970293595999, 0.11813285955434884, 0.18560978107628873, -0.1624212287078026])
        self.trajGoToIdle.addPostureWP([0.09592785524414665, 0.09592785524414665, 
            0.581393160586458, -0.06765800596877417, 0.11796117340291093, 1.6240955805165236, 0.03394658862046966, 0.20374019828081738, -0.1572576379385714,
            0.581393160586458, -0.06765800596877417, 0.11796117340291093, 1.6240955805165236, 0.03394658862046966, 0.20374019828081738, -0.1572576379385714])
        self.trajGoToIdle.addPostureWP([0.09643131954418892, 0.09643131954418892, 
            0.581393160586458, -0.06765800596877417, 0.11796117340291093, 1.6240955805165236, 0.03394658862046966, 0.20374019828081738, -0.1572576379385714,
            0.581393160586458, -0.06765800596877417, 0.11796117340291093, 1.6240955805165236, 0.03394658862046966, 0.20374019828081738, -0.1572576379385714])
        self.trajGoToIdle.addPostureWP([0.09559710847417426, 0.09559710847417426, 
            0.18493035171345457, -0.018093572235340995, 0.12375741975377275, 0.7804877049806426, -0.12790804246025334, 0.06510619728499865, -0.04870942823921384,
            0.18493035171345457, -0.018093572235340995, 0.12375741975377275, 0.7804877049806426, -0.12790804246025334, 0.06510619728499865, -0.04870942823921384])

    def run(self):
        """
        Runs the Cartesian and orientation demo 1 behavior.
        """

        if not self.dreamerInterface.connectToControlIt(DEFAULT_POSTURE):
            return

        self.createTrajectories()

        # print "Trajectory Wave:\n {0}".format(self.trajWave)

        response = raw_input("Start demo? Y/n\n")
        if response == "N" or response == "n":
            return

        #=============================================================================
        if not self.dreamerInterface.followTrajectory(self.trajGoToReady):
            return

        #=============================================================================
        done = False
        while not done:
            if not self.dreamerInterface.followTrajectory(self.trajWave):
                return

            response = raw_input("Wave again? Y/n\n")
            if response == "N" or response == "n":
                done = True

        #=============================================================================
        if not self.dreamerInterface.followTrajectory(self.trajGoToIdle):
            return

# Main method
if __name__ == "__main__":

    rospy.init_node('Demo4_HandWave', anonymous=True)

    demo = Demo4_HandWave()
    # t = threading.Thread(target=demo.run)
    # t.start()
    demo.run()

    print "Demo 4 done, waiting until ctrl+c is hit..."
    rospy.spin()  # just to prevent this node from exiting