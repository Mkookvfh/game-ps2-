TARGET = game_test
SRC = src/main.c src/player.c src/combat.c src/ai_npc.c

all:
	gcc $(SRC) -o $(TARGET) -lm

clean:
	rm -f $(TARGET)
