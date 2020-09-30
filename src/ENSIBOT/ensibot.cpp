//#pragma once
// api.cpp : Defines the exported functions for the DLL.
//

#include "ensibot.h"

using namespace std;
using namespace std::literals::chrono_literals;



// Socket servers
int server_socket, client_socket;

// Reward computation
float reward;

bool is_client_connected = false;

enum bones_ids {
    head = 0,
    neck = 2,

    spine_1 = 4,
    spine_2 = 5,
    spine_3 = 6,
    spine_4 = 7,

    left_upper_arm = 18,
    left_forearm = 19,
    left_and = 15,

    right_upper_arm = 16,
    right_forearm = 17,
    right_and = 14,

    left_thigh = 9,
    left_calf = 11,
    left_foot = 13,

    right_thigh = 8,
    right_calf = 10,
    right_foot = 12,
};

// Me (as a player entity).
C_BasePlayer* p_local_player;
int my_index;
TeamID my_team;
C_BaseCombatWeapon* p_my_weapon;
Vector point_my_position;
QAngle my_view_angles;
Vector vec_aim_direction;


// SOCKET UTILITIES

void CreateMove(CUserCmd* pCmd);

void CreateServer();

void HandleMessage(string message);

// REWARD

void ComputeReward(CUserCmd* pCmd);

Vector QAngleToVector(QAngle qAngle);

bool ComputeIfHit();

void ComputeClosest();

bool IsEnnemyValid(C_BaseEntity* ennemy_entity);

void sendReward();



// Public functions

void EnsiBot::Init() {
    reward = 0.f;
    is_client_connected = false;
    CreateServer();
}


void EnsiBot::CreateMove(CUserCmd* pCmd) {

    ComputeReward(pCmd);

    // receiving message
    if (is_client_connected) {
        char buffer[1024];

        ssize_t receive_size = recv(client_socket, buffer, sizeof(buffer), 0);

        if (receive_size > 0)//10035
        {
            HandleMessage(string(buffer));
            // Clearing buffer
            memset(buffer, 0, sizeof(buffer));
        }
    }
}


void CreateServer() {

    //WSADATA WSAData;
    //WSAStartup(MAKEWORD(2, 0), &WSAData);
    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(3121);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    int server_socket_status = bind(server_socket,
                                    (sockaddr*)&serverAddr,
                                    (socklen_t) sizeof(serverAddr));

    listen(server_socket, 0);

    cvar->ConsoleDPrintf("Listening for incoming connections...\n");

    char buffer[1024];
    string message;
    sockaddr_in clientAddr = {};
    socklen_t clientAddrSize = sizeof(clientAddr);
    if ((client_socket = accept(server_socket,
                                (sockaddr*)& clientAddr,
                                &clientAddrSize)) != -1)
    {
        // Setting the socket to non-blocking mode
        fcntl(client_socket, F_SETFL, O_NONBLOCK);
        cvar->ConsoleDPrintf("Client connected !\n");
        is_client_connected = true;
    }
}

void HandleMessage(string message) {
    if (message.length() == 0) {
        cvar->ConsoleDPrintf("Message is none.\n");
    }

    else if (message == "close") {
        sendReward();
    }

    else if (message == "get_reward") {
        sendReward();
    }

    else if (message == "kill_bots") {
        // TODO send command "kill bots"
        cvar->ConsoleDPrintf("Killing bots.\n");
    }
    else {
        cvar->ConsoleDPrintf("Unknown message : \n");
        cvar->ConsoleDPrintf("\tLength = %i\n", message.length());
        cvar->ConsoleDPrintf("\tpayload = %s\n", message);
    }
}

Vector QAngleToVector(QAngle qAngle) {

    Vector forward = Vector();

    float sp, sy, cp, cy;
    float pitch = DEG2RAD(qAngle.x);
    float yaw = DEG2RAD(qAngle.y);

    sp = sin(pitch);
    cp = cos(pitch);

    sy = sin(yaw);
    cy = cos(yaw);

    forward.x = cp * cy;
    forward.y = cp * sy;
    forward.z = -sp;

    return forward.Normalize();
}

void ComputeReward(CUserCmd* pCmd) {

    // Updating player info
    my_index = engine->GetLocalPlayer();
    p_local_player = (C_BasePlayer *) entityList->GetClientEntity(my_index);
    my_team = p_local_player->GetTeam();

    my_view_angles = pCmd->viewangles;
    vec_aim_direction = QAngleToVector(my_view_angles);
    point_my_position = p_local_player->GetEyePosition();

    // Computing reward value
    if (!ComputeIfHit()) // if an ennemy player is being aimed at
        ComputeClosest(); // if no ennemy player is being aimed at

    //cvar->ConsoleDPrintf("reward = %f\n", reward);
}

bool ComputeIfHit() {

    // Checking if we hold a valid weapon
	C_BaseCombatWeapon* p_my_weapon = ( C_BaseCombatWeapon* ) entityList->GetClientEntityFromHandle(
            p_local_player->GetActiveWeapon() );
    if (!p_my_weapon)
        return false;

    CTraceFilter filter;
    filter.pSkip = p_local_player;
    trace_t trace_result;
    Ray_t ray;
    ray.Init(
        point_my_position,
        point_my_position + vec_aim_direction * (p_my_weapon->GetCSWpnData()->GetRange()));

    trace->TraceRay(ray, 0x46004003, &filter, &trace_result);

    if (!IsEnnemyValid(trace_result.m_pEntityHit)) {
        //cvar->ConsoleDPrintf("No ennemy.\n");
        return false;
    }

    switch (trace_result.hitbox) {
    case bones_ids::head:
        reward = 1.f;
        break;
    default:
        reward = 0.5;
    }
    return true;
}

void ComputeClosest() {

    float min_dist = FLT_MAX;

    C_BaseEntity* p_ennemy_entity;
    float dist_to_ennemy = 0.f;

    for (int ennemy_player_index = 0; ennemy_player_index < engine->GetMaxClients(); ennemy_player_index++) {

        // Checking if the considered player is different from me
        if (ennemy_player_index == my_index)
            continue;

        p_ennemy_entity = entityList->GetClientEntity(ennemy_player_index);

        if (!IsEnnemyValid(p_ennemy_entity))
            continue;

        // Computing distance

        // Point E
        Vector point_ennemy_position = ((C_BasePlayer *) p_ennemy_entity)->GetEyePosition();

        // Vector OE
        Vector vec_me_to_ennemy = point_ennemy_position - point_my_position;

        // Vector OP
        Vector vec_projection = vec_aim_direction * (vec_me_to_ennemy.Dot(vec_aim_direction));

        // Norm ||OP - OE||
        dist_to_ennemy = vec_me_to_ennemy.DistTo(vec_projection);

        if (dist_to_ennemy < min_dist) {
            min_dist = dist_to_ennemy;
        }
    }

    // Computing reward
    //reward = 0.5 * exp(- min_dist);
    reward = 0.5 - min_dist;
}

bool IsEnnemyValid(C_BaseEntity* ennemy_entity) {

    if (!ennemy_entity) {
        //cvar->ConsoleDPrintf("Null pointer.\n");
        return false;
    }

    if (ennemy_entity->GetDormant()) {
        //cvar->ConsoleDPrintf("Is dormant.\n");
        return false;
    }

    if (!ennemy_entity->IsPlayer()){
        //cvar->ConsoleDPrintf("Is not a player.\n");
        return false;
    }

    C_BasePlayer* ennemy_player = (C_BasePlayer*) ennemy_entity;

    if (!ennemy_player->GetAlive()){
        //cvar->ConsoleDPrintf("Is not alive.\n");
        return false;
    }

    TeamID ennemy_team = ennemy_entity->GetTeam();

    if (ennemy_team != TeamID::TEAM_TERRORIST && ennemy_team != TeamID::TEAM_COUNTER_TERRORIST) {
        //cvar->ConsoleDPrintf("Is neither a T nor a CT.\n");
        return false;
    }

    if (ennemy_team == my_team) {
        //cvar->ConsoleDPrintf("Is in my team.\n");
        return false;
    }

    // TODO check if aiming an immune ennemy is bad !
    if (ennemy_player->GetImmune()) {
        //cvar->ConsoleDPrintf("Is immune.\n");
        return false;
    }

    return true;
}

void sendReward() {

    //cout << "sending reward to client" << endl;
    cvar->ConsoleDPrintf("sending reward to client\n", reward);

    cvar->ConsoleDPrintf("reward = %f\n", reward);

    send(client_socket, (const char *) &reward, sizeof reward, 0);

    cvar->ConsoleDPrintf("reward sent to client.\n");
}
