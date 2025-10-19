#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Trabalho Leitura Imagem Ext2
// Alunos: Lucas B. Santos

#define INODE_SIZE 128 // Tamanho padrão do inode em ext2

typedef struct superblock
{
	unsigned int
		total_inodes,
		total_blocks,
		reserved_blocks,
		free_blocks,
		free_inodes,
		sb_block_number,
		block_size,
		fragment_size,
		blocks_per_group,
		fragments_per_group,
		inodes_per_group;

} __attribute__((packed)) superblock_t;

typedef struct group_descriptor
{
	unsigned int
		block_bitmap_block,
		inode_bitmap_block,
		inode_table_start_block;

} __attribute__((packed)) group_descriptor_t;

typedef struct inode
{
	unsigned short mode;
	unsigned short uid;
	unsigned int size;
	unsigned int atime;
	unsigned int ctime;
	unsigned int mtime;
	unsigned int dtime;
	unsigned short gid;
	unsigned short links_count;
	unsigned int blocks;
	unsigned int flags;
	unsigned int osd1;
	unsigned int block[15];
	unsigned int generation;
	unsigned int file_acl;
	unsigned int dir_acl;
	unsigned int faddr;
	unsigned char osd2[12];

} __attribute__((packed)) inode_t;

typedef struct directory_entry
{
	unsigned int inode;
	unsigned short rec_len;
	unsigned char name_len;
	unsigned char file_type;
	char name[255];

} __attribute__((packed)) directory_entry_t;

void read_file(FILE *fp, superblock_t superblock, inode_t inode)
{
	unsigned int block_size = 1024 << superblock.block_size;
	unsigned int file_size = inode.size;
	unsigned int total_blocks = (file_size + block_size - 1) / block_size;

	for (unsigned int i = 0; i < total_blocks && i < 12; i++) // Apenas blocos diretos
	{
		unsigned int block_num = inode.block[i];
		unsigned int block_offset = block_num * block_size;

		fseek(fp, block_offset, SEEK_SET);
		printf("\nConteudo do Bloco %u:\n", block_num);
		for (unsigned int j = 0; j < block_size && (i * block_size + j) < file_size; j++)
		{
			char c;
			fread(&c, sizeof(char), 1, fp);
			putchar(c);
		}
	}
}

void read_directory(FILE *fp, superblock_t superblock, inode_t inode, int read_content)
{
	unsigned int block_size = 1024 << superblock.block_size;
	unsigned int blocks_per_group = superblock.blocks_per_group;
	unsigned int inodes_per_group = superblock.inodes_per_group;
	unsigned int gd_block = block_size != 1024 ? 1 : 2; // esse número é relativo ao BG que se está analisando
	unsigned int total_blocks = (inode.size + block_size - 1) / block_size;

	for (unsigned int i = 0; i < total_blocks && i < 12; i++) // Apenas blocos diretos
	{

		unsigned int pointer = inode.block[i];

		printf("Pointer bloco direto %u: %i \n", i, pointer);

		// Ponteiros diretos possuem o número absoluto do bloco.
		// Portanto, basta multiplicar pelo tamanho (bytes) para descobrir o deslocamento
		int dir_block_offset = pointer * block_size;

		directory_entry_t entry;

		fseek(fp, dir_block_offset, SEEK_SET);

		int bytes_read = 0;

		while (bytes_read < inode.size)
		{
			fread(&entry, sizeof(directory_entry_t), 1, fp);
			entry.name[entry.name_len] = '\0'; // Null-terminate the name string

			unsigned int current_inode_number = entry.inode;
			unsigned int group_number = (current_inode_number - 1) / inodes_per_group;
			unsigned int index_in_table = (current_inode_number - 1) % inodes_per_group;

			// Deslocamento dentro do block group + deslocamento para chegar naquele block group
			int gd_offset = gd_block * block_size + group_number * blocks_per_group * block_size;

			fseek(fp, gd_offset, SEEK_SET);
			group_descriptor_t group_descriptor;
			fread(&group_descriptor, sizeof(group_descriptor_t), 1, fp);

			unsigned int inode_table_block = group_descriptor.inode_table_start_block;

			// Deslocamento para chegar no block group + deslocamento para chegar na tabela + deslocamento dentro da tabela
			unsigned int inode_offset = group_number * blocks_per_group * block_size + inode_table_block * block_size + index_in_table * INODE_SIZE;

			inode_t current_inode;
			fseek(fp, inode_offset, SEEK_SET);
			fread(&current_inode, sizeof(inode_t), 1, fp);

			printf("Inode: %i, Nome: %s\n", entry.inode, entry.name);

			// 1 indica arquivo regular
			if (read_content && entry.file_type == 1)
			{
				read_file(fp, superblock, current_inode);
			}

			// 2 indica diretório
			else if (entry.file_type == 2 && entry.name[0] != '.' && strcmp(entry.name, "lost+found") != 0)
			{
				printf("\n==== Lendo subdiretório: %s ======\n", entry.name);
				read_directory(fp, superblock, current_inode, read_content);
				printf("\n==== Fim do subdiretório: %s ======\n", entry.name);
			}

			// Devemos acumular o tamanho da entrada para ir descobrindo o deslocamento da entrada seguinte
			bytes_read += entry.rec_len;
			fseek(fp, dir_block_offset + bytes_read, SEEK_SET);
		}
	}
}

int main(int argc, char *argv[])
{
	int read_content = 0;
	if (argc < 2)
	{
		printf("Uso: %s <ext2_img_file> [-r] \n", argv[0]);
		printf("-r: ler conteudo dos arquivos\n");
		return 1;
	}
	else if (argc == 3 && strcmp(argv[2], "-r") == 0)
	{
		read_content = 1;
	}

	FILE *fp;
	superblock_t superblock;
	group_descriptor_t group_descriptor;

	fp = fopen(argv[1], "rb");

	if (fp == NULL)
	{
		perror("Erro ao abrir o arquivo");
		return 1;
	}

	fseek(fp, 1024, SEEK_SET);
	fread(&superblock, sizeof(superblock_t), 1, fp);

	unsigned int block_size = 1024 << superblock.block_size;
	unsigned int sb_block_number = superblock.sb_block_number;
	unsigned int inodes_per_group = superblock.inodes_per_group;
	unsigned int blocks_per_group = superblock.blocks_per_group;

	int total_block_groups = (superblock.total_blocks + blocks_per_group - 1) / blocks_per_group;

	printf("total inodes: %i \n", superblock.total_inodes);
	printf("total blocks: %i \n", superblock.total_blocks);
	printf("total block groups: %i \n", total_block_groups);
	printf("SB number: %i \n", sb_block_number);
	printf("Tamanho de bloco (bytes): %i \n", block_size);
	printf("Blocks per group: %i \n", blocks_per_group);
	printf("Inodes per group: %i \n", inodes_per_group);

	int gd_block = block_size != 1024 ? 1 : 2;

	fseek(fp, gd_block * block_size, SEEK_SET);
	fread(&group_descriptor, sizeof(group_descriptor_t), 1, fp);
	int inode_table_block = group_descriptor.inode_table_start_block;

	// inode do root dir é o 2 por padrão
	int inode_number = 2;

	// Descobrir qual a posição na tabela.
	// Usa-se o -1 visto que inodes são numerados a partir de 1 e não 0
	int index_in_table = (inode_number - 1) % inodes_per_group;

	// Deslocamento para chegar na tabela + deslocamento dentro da tabela
	int offset = inode_table_block * block_size + index_in_table * INODE_SIZE;

	inode_t inode;
	fseek(fp, offset, SEEK_SET);
	fread(&inode, sizeof(inode_t), 1, fp);
	printf("===== LENDO DIRETORIO RAIZ =====\n");
	
	// Disparar leitura a partir do inode do diretorio raiz
	read_directory(fp, superblock, inode, read_content); 
	fclose(fp);
	return 0;
}
